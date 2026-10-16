package com.webgpu;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.PixelCopy;
import android.view.Surface;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Reads the frame a surface shows back into a bitmap. A SurfaceView's surface
 * goes with its buffers when the app goes to the background or the view leaves
 * its window, and SurfaceHolder.Callback.surfaceDestroyed is the last moment
 * that frame can be read; the canvas keeps the copy for its next surface.
 */
final class SurfaceFrameCapture {

  // The UI thread waits for the read; one that has not completed by then is
  // given up, and the canvas comes back with the next frame it draws.
  private static final long MAX_WAIT_MILLIS = 1000;

  private static HandlerThread sListenerThread;

  private SurfaceFrameCapture() {}

  /** The surface's frame at width x height, or null if it could not be read. Main thread. */
  @Nullable
  static Bitmap capture(@NonNull Surface surface, int width, int height) {
    if (width <= 0 || height <= 0 || !surface.isValid()) {
      return null;
    }
    Bitmap frame = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    CountDownLatch done = new CountDownLatch(1);
    int[] result = {PixelCopy.ERROR_UNKNOWN};
    PixelCopy.request(surface, frame, copyResult -> {
      result[0] = copyResult;
      done.countDown();
    }, listenerHandler());
    try {
      if (!done.await(MAX_WAIT_MILLIS, TimeUnit.MILLISECONDS)) {
        frame.recycle();
        return null;
      }
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      frame.recycle();
      return null;
    }
    if (result[0] != PixelCopy.SUCCESS) {
      frame.recycle();
      return null;
    }
    return frame;
  }

  // PixelCopy reports on a looper; the main thread is the one waiting here.
  private static synchronized Handler listenerHandler() {
    if (sListenerThread == null) {
      sListenerThread = new HandlerThread("WebGPUFrameCapture");
      sListenerThread.start();
    }
    return new Handler(sListenerThread.getLooper());
  }
}
