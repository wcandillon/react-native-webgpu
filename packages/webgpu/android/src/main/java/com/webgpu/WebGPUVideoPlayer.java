package com.webgpu;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.HardwareBuffer;
import android.media.AudioAttributes;
import android.media.Image;
import android.media.ImageReader;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import com.facebook.proguard.annotations.DoNotStrip;

import java.io.File;
import java.io.IOException;
import java.util.HashSet;
import java.util.Set;

/**
 * Decodes a video with {@link MediaPlayer} into an {@link ImageReader} whose
 * buffers the GPU can sample ({@link ImageFormat#PRIVATE} with
 * {@link HardwareBuffer#USAGE_GPU_SAMPLED_IMAGE}). Each frame is handed to C++
 * (AndroidVideoPlayer) as an {@link Image} that stays acquired until the
 * NativeVideoFrame is released, which hands the buffer back to the decoder.
 *
 * The frames keep the decoder's native YUV layout, so they are sampled through
 * GPUDevice.importExternalTexture. MediaPlayer owns the audio, the clock and
 * the A/V synchronization.
 *
 * MediaPlayer is driven from the caller's thread (its methods are thread
 * safe); its callbacks arrive on the main looper, so the shared state is
 * guarded by {@code lock}.
 */
@DoNotStrip
public class WebGPUVideoPlayer {
  private static final String TAG = "WebGPUVideoPlayer";

  // Frames the consumer can hold at once: the one being drawn, the next one,
  // and a spare. Holding more makes copyLatestFrame() return null until a
  // frame is released.
  private static final int MAX_IMAGES = 3;

  private final Object lock = new Object();
  private final Set<Image> acquired = new HashSet<>();
  private MediaPlayer player;
  private ImageReader reader;
  private boolean prepared = false;
  private boolean playing = false;
  private boolean loop = true;
  private float volume = 1f;
  private double pendingSeekSeconds = -1;
  private boolean released = false;

  private volatile int width = 0;
  private volatile int height = 0;
  private volatile int rotation = 0;
  private volatile double frameRate = 0;
  private volatile double durationSeconds = 0;

  WebGPUVideoPlayer(Context context, String source) {
    Uri uri = parseSource(source);
    readMetadata(context, uri);

    MediaPlayer mp = new MediaPlayer();
    mp.setAudioAttributes(
        new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
            .build());
    mp.setOnVideoSizeChangedListener((p, w, h) -> onVideoSizeChanged(w, h));
    mp.setOnPreparedListener(this::onPrepared);
    mp.setOnCompletionListener(
        p -> {
          // Not called while looping: MediaPlayer restarts on its own.
          synchronized (lock) {
            if (!loop) {
              playing = false;
            }
          }
        });
    mp.setOnErrorListener(
        (p, what, extra) -> {
          Log.e(TAG, "MediaPlayer error " + what + " (" + extra + ") for " + source);
          synchronized (lock) {
            playing = false;
          }
          return true;
        });
    try {
      mp.setDataSource(context, uri);
    } catch (IOException | IllegalArgumentException | SecurityException e) {
      mp.release();
      throw new RuntimeException("createVideoPlayer: cannot open " + source, e);
    }
    mp.setLooping(loop);
    mp.prepareAsync();
    synchronized (lock) {
      player = mp;
    }
  }

  // A URL (http, https, file, content) or a plain file path.
  private static Uri parseSource(String source) {
    Uri uri = Uri.parse(source);
    if (uri.getScheme() == null) {
      return Uri.fromFile(new File(source));
    }
    return uri;
  }

  // The frame geometry comes from the container (MediaMetadataRetriever for
  // the size, rotation and duration, MediaExtractor for the frame rate). The
  // metadata of a remote video is fetched off the calling thread; the getters
  // report 0 until it is known.
  private void readMetadata(Context context, Uri uri) {
    Thread thread =
        new Thread(
            () -> {
              MediaMetadataRetriever retriever = new MediaMetadataRetriever();
              try {
                String scheme = uri.getScheme();
                if ("http".equals(scheme) || "https".equals(scheme)) {
                  retriever.setDataSource(uri.toString(), new java.util.HashMap<>());
                } else {
                  retriever.setDataSource(context, uri);
                }
                int w = parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH));
                int h = parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT));
                if (w > 0 && h > 0 && width == 0) {
                  width = w;
                  height = h;
                }
                rotation = parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION));
                long durationMs = parseInt(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION));
                if (durationMs > 0 && durationSeconds == 0) {
                  durationSeconds = durationMs / 1000.0;
                }
              } catch (RuntimeException e) {
                Log.w(TAG, "Could not read the metadata of " + uri + ": " + e.getMessage());
              } finally {
                try {
                  retriever.release();
                } catch (IOException ignored) {
                  // Nothing to recover.
                }
              }

              MediaExtractor extractor = new MediaExtractor();
              try {
                extractor.setDataSource(context, uri, null);
                for (int i = 0; i < extractor.getTrackCount(); i++) {
                  MediaFormat format = extractor.getTrackFormat(i);
                  String mime = format.getString(MediaFormat.KEY_MIME);
                  if (mime != null && mime.startsWith("video/")) {
                    if (format.containsKey(MediaFormat.KEY_FRAME_RATE)) {
                      frameRate = format.getInteger(MediaFormat.KEY_FRAME_RATE);
                    }
                    break;
                  }
                }
              } catch (IOException | RuntimeException e) {
                Log.w(TAG, "Could not read the frame rate of " + uri + ": " + e.getMessage());
              } finally {
                extractor.release();
              }
            },
            "WebGPUVideoPlayer metadata");
    thread.setDaemon(true);
    thread.start();
  }

  private static int parseInt(String value) {
    if (value == null) {
      return 0;
    }
    try {
      return Integer.parseInt(value);
    } catch (NumberFormatException e) {
      return 0;
    }
  }

  // The decoder reports the frame size (possibly more than once for adaptive
  // streams); the ImageReader that receives the frames is sized accordingly.
  private void onVideoSizeChanged(int w, int h) {
    if (w <= 0 || h <= 0) {
      return;
    }
    ImageReader previous;
    synchronized (lock) {
      if (released || player == null) {
        return;
      }
      if (reader != null && reader.getWidth() == w && reader.getHeight() == h) {
        return;
      }
      if (width == 0) {
        width = w;
        height = h;
      }
      previous = reader;
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        reader = ImageReader.newInstance(w, h, ImageFormat.PRIVATE, MAX_IMAGES, HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE);
      } else {
        reader = ImageReader.newInstance(w, h, ImageFormat.PRIVATE, MAX_IMAGES);
      }
      player.setSurface(reader.getSurface());
    }
    if (previous != null) {
      // Frames still held by the consumer were acquired from the old reader;
      // closing it closes them, and the native side keeps the buffers alive.
      synchronized (lock) {
        acquired.clear();
      }
      previous.close();
    }
  }

  private void onPrepared(MediaPlayer mp) {
    synchronized (lock) {
      if (released) {
        return;
      }
      prepared = true;
      int durationMs = mp.getDuration();
      if (durationMs > 0) {
        durationSeconds = durationMs / 1000.0;
      }
      mp.setVolume(volume, volume);
      mp.setLooping(loop);
      if (pendingSeekSeconds >= 0) {
        mp.seekTo((long) (pendingSeekSeconds * 1000), MediaPlayer.SEEK_CLOSEST);
        pendingSeekSeconds = -1;
      }
      if (playing) {
        mp.start();
      }
    }
  }

  /**
   * The most recently decoded frame, or null when no new frame was decoded
   * since the last call (or when the consumer holds {@link #MAX_IMAGES}
   * frames). Close it with {@link #closeImage} once the frame is released.
   */
  @DoNotStrip
  public Image copyLatestFrame() {
    ImageReader current;
    synchronized (lock) {
      if (released || reader == null) {
        return null;
      }
      current = reader;
    }
    Image image;
    try {
      image = current.acquireLatestImage();
    } catch (IllegalStateException e) {
      Log.w(TAG, "copyLatestFrame: every frame is held, release one to get a new frame");
      return null;
    }
    if (image == null) {
      return null;
    }
    synchronized (lock) {
      acquired.add(image);
    }
    return image;
  }

  /** Hands the buffer of a frame returned by {@link #copyLatestFrame} back to the decoder. */
  @DoNotStrip
  public void closeImage(Image image) {
    synchronized (lock) {
      if (!acquired.remove(image)) {
        return;
      }
    }
    image.close();
  }

  @DoNotStrip
  public void play() {
    synchronized (lock) {
      playing = true;
      if (player != null && prepared) {
        player.start();
      }
    }
  }

  @DoNotStrip
  public void pause() {
    synchronized (lock) {
      playing = false;
      if (player != null && prepared && player.isPlaying()) {
        player.pause();
      }
    }
  }

  @DoNotStrip
  public boolean getPaused() {
    synchronized (lock) {
      return !playing;
    }
  }

  @DoNotStrip
  public double getCurrentTime() {
    synchronized (lock) {
      if (player != null && prepared) {
        return player.getCurrentPosition() / 1000.0;
      }
      return pendingSeekSeconds >= 0 ? pendingSeekSeconds : 0;
    }
  }

  // An exact seek, also while paused: the decoder then renders the frame at
  // the new position, which the next copyLatestFrame() returns.
  @DoNotStrip
  public void seek(double seconds) {
    synchronized (lock) {
      if (player != null && prepared) {
        player.seekTo((long) (Math.max(0, seconds) * 1000), MediaPlayer.SEEK_CLOSEST);
      } else {
        pendingSeekSeconds = Math.max(0, seconds);
      }
    }
  }

  @DoNotStrip
  public double getDuration() {
    return durationSeconds;
  }

  @DoNotStrip
  public double getVolume() {
    synchronized (lock) {
      return volume;
    }
  }

  @DoNotStrip
  public void setVolume(double value) {
    synchronized (lock) {
      volume = (float) Math.max(0, Math.min(1, value));
      if (player != null) {
        player.setVolume(volume, volume);
      }
    }
  }

  @DoNotStrip
  public boolean getLoop() {
    synchronized (lock) {
      return loop;
    }
  }

  @DoNotStrip
  public void setLoop(boolean value) {
    synchronized (lock) {
      loop = value;
      if (player != null) {
        player.setLooping(value);
      }
    }
  }

  @DoNotStrip
  public int getVideoWidth() {
    return width;
  }

  @DoNotStrip
  public int getVideoHeight() {
    return height;
  }

  @DoNotStrip
  public int getRotation() {
    return rotation;
  }

  @DoNotStrip
  public double getFrameRate() {
    return frameRate;
  }

  /** Stops playback and releases the decoder. Frames already copied stay valid until closed. */
  @DoNotStrip
  public void release() {
    MediaPlayer mp;
    ImageReader current;
    synchronized (lock) {
      if (released) {
        return;
      }
      released = true;
      playing = false;
      mp = player;
      player = null;
      current = reader;
      reader = null;
      acquired.clear();
    }
    if (mp != null) {
      mp.release();
    }
    if (current != null) {
      current.close();
    }
  }
}
