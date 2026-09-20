package com.webgpu;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorSpace;
import android.graphics.HardwareRenderer;
import android.graphics.PixelFormat;
import android.graphics.RecordingCanvas;
import android.graphics.RenderNode;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.util.Log;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

/**
 * Rasterizes a native view for GPUQueue.drawElementImageToTexture. Must be
 * called on the UI thread.
 *
 * On API 29+ the view is rendered through {@link HardwareRenderer}: the
 * recording reuses the children's existing display lists, so the result is
 * exactly what the window's own renderer draws (shadows, hardware layers,
 * TextureView content). Older devices, or a failed hardware render, fall back
 * to drawing the view into a software canvas, which misses anything that is
 * only drawn hardware-accelerated. SurfaceView content is composed outside
 * the view hierarchy and is captured by neither path.
 */
final class ViewSnapshot {
  private static final String TAG = "WebGPUViewSnapshot";

  private ViewSnapshot() {}

  /**
   * @param sourceX/Y/Width/Height the source rectangle in the view's own
   *     coordinate space, in pixels (already scaled by the display density)
   * @param width/height the output size in pixels
   * @return a software ARGB_8888 (premultiplied) bitmap of exactly
   *     width x height pixels
   */
  @NonNull
  static Bitmap snapshot(
      @NonNull View view,
      float sourceX,
      float sourceY,
      float sourceWidth,
      float sourceHeight,
      int width,
      int height) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && view.isHardwareAccelerated()) {
      try {
        return snapshotHardware(view, sourceX, sourceY, sourceWidth, sourceHeight, width, height);
      } catch (Throwable t) {
        Log.w(TAG, "HardwareRenderer snapshot failed, falling back to software rendering", t);
      }
    }
    return snapshotSoftware(view, sourceX, sourceY, sourceWidth, sourceHeight, width, height);
  }

  @NonNull
  private static Bitmap snapshotSoftware(
      @NonNull View view,
      float sourceX,
      float sourceY,
      float sourceWidth,
      float sourceHeight,
      int width,
      int height) {
    Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(bitmap);
    canvas.scale(width / sourceWidth, height / sourceHeight);
    canvas.translate(-sourceX, -sourceY);
    view.draw(canvas);
    return bitmap;
  }

  @RequiresApi(Build.VERSION_CODES.Q)
  @NonNull
  private static Bitmap snapshotHardware(
      @NonNull View view,
      float sourceX,
      float sourceY,
      float sourceWidth,
      float sourceHeight,
      int width,
      int height) {
    // The renderer draws into an ImageReader whose buffers are GPU-sampleable
    // (a later zero-copy path can import them directly); for now the frame is
    // read back into a software bitmap.
    ImageReader reader =
        ImageReader.newInstance(
            width,
            height,
            PixelFormat.RGBA_8888,
            1,
            HardwareBuffer.USAGE_GPU_SAMPLED_IMAGE | HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
    HardwareRenderer renderer = new HardwareRenderer();
    Image image = null;
    try {
      renderer.setSurface(reader.getSurface());
      // Match the framework's default light so elevation shadows come out
      // like on screen (ThreadedRenderer reads these from the theme; the
      // Material defaults are used here).
      float density = view.getResources().getDisplayMetrics().density;
      renderer.setLightSourceGeometry(
          width / 2f, -400f * density, 500f * density, 800f * density);
      renderer.setLightSourceAlpha(0.039f, 0.19f);

      RenderNode root = new RenderNode("rnwgpu-view-snapshot");
      root.setPosition(0, 0, width, height);
      RecordingCanvas canvas = root.beginRecording(width, height);
      try {
        canvas.scale(width / sourceWidth, height / sourceHeight);
        canvas.translate(-sourceX, -sourceY);
        view.draw(canvas);
      } finally {
        root.endRecording();
      }
      renderer.setContentRoot(root);

      HardwareRenderer.FrameRenderRequest request = renderer.createRenderRequest();
      request.setWaitForPresent(true);
      int status = request.syncAndDraw();
      int failed =
          HardwareRenderer.SYNC_LOST_SURFACE_REWARD_IF_FOUND
              | HardwareRenderer.SYNC_CONTEXT_IS_STOPPED
              | HardwareRenderer.SYNC_FRAME_DROPPED;
      if ((status & failed) != 0) {
        throw new IllegalStateException("HardwareRenderer.syncAndDraw failed with status " + status);
      }
      image = reader.acquireNextImage();
      if (image == null) {
        throw new IllegalStateException("HardwareRenderer produced no image");
      }
      HardwareBuffer buffer = image.getHardwareBuffer();
      if (buffer == null) {
        throw new IllegalStateException("HardwareRenderer image has no HardwareBuffer");
      }
      Bitmap hardware =
          Bitmap.wrapHardwareBuffer(buffer, ColorSpace.get(ColorSpace.Named.SRGB));
      buffer.close();
      if (hardware == null) {
        throw new IllegalStateException("Bitmap.wrapHardwareBuffer failed");
      }
      Bitmap software = hardware.copy(Bitmap.Config.ARGB_8888, false);
      hardware.recycle();
      if (software == null) {
        throw new IllegalStateException("could not read back the hardware bitmap");
      }
      return software;
    } finally {
      if (image != null) {
        image.close();
      }
      renderer.destroy();
      reader.close();
    }
  }
}
