package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

/**
 * A {@link SurfaceView} whose frames reach the compositor with GPU fences in
 * both directions (Android 10 and above).
 *
 * <p>WebGPU renders into the same native pool of AHardwareBuffers as {@link
 * WebGPUHardwareBufferView} (see SurfaceRegistry.h), but each finished buffer
 * is posted by native code to a child surface of this view's window
 * ({@code ASurfaceControl}) together with its render-complete fence, so no
 * thread waits for the GPU; when the compositor latches the next frame, it
 * hands back the fence that signals once it is done reading the previous
 * buffer, and the next render into that buffer waits on it on the GPU. This
 * view only owns the window and reports its size; it never draws.
 *
 * <p>Like any SurfaceView it is its own compositor layer, outside the view
 * hierarchy (no parent transforms or clipping); unlike the default
 * SurfaceView it has no swapchain, so a frame is never blocked behind
 * vsync at present time. When the platform cannot create the child surface,
 * the view's own surface is used as a plain swapchain instead.
 */
@RequiresApi(api = Build.VERSION_CODES.Q)
@SuppressLint("ViewConstructor")
public class WebGPUSurfaceControlView extends SurfaceView implements SurfaceHolder.Callback {

  private final WebGPUAPI mApi;
  private boolean mOpaque;
  // The child surface could not be created: this view's surface is the
  // swapchain, through the regular surface callbacks.
  private boolean mFallback;
  private boolean mAttached;

  public WebGPUSurfaceControlView(Context context, WebGPUAPI api, boolean zOrderOnTop, boolean opaque) {
    super(context);
    mApi = api;
    mOpaque = opaque;
    setZOrderOnTop(zOrderOnTop);
    getHolder().setFormat(opaque ? PixelFormat.OPAQUE : PixelFormat.TRANSLUCENT);
    getHolder().addCallback(this);
  }

  private int contextId() {
    return mApi.getContextId();
  }

  private int dp(int px) {
    final float density = getResources().getDisplayMetrics().density;
    return Math.max(1, Math.round(px / density));
  }

  /** The child surface's own transparency follows; the holder format drives the fallback's. */
  public void setOpaque(boolean opaque) {
    mOpaque = opaque;
    getHolder().setFormat(opaque ? PixelFormat.OPAQUE : PixelFormat.TRANSLUCENT);
    if (mAttached && !mFallback) {
      nSetOpaque(contextId(), opaque);
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    // surfaceDestroyed() normally fires during detach as well; going offscreen
    // is idempotent, so this is just a safety net for paths where it does not.
    goOffscreen();
  }

  private void goOffscreen() {
    if (mFallback) {
      mApi.surfaceOffscreen();
    } else if (mAttached) {
      nDetach(contextId());
    }
    mAttached = false;
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    Surface surface = holder.getSurface();
    int w = Math.max(1, getWidth());
    int h = Math.max(1, getHeight());
    if (nAttach(contextId(), surface, w, h, dp(w), dp(h), mOpaque)) {
      mFallback = false;
      mAttached = true;
    } else {
      mFallback = true;
      mApi.surfaceCreated(surface);
    }
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    if (mFallback) {
      mApi.surfaceChanged(holder.getSurface());
    } else {
      nResize(contextId(), Math.max(1, width), Math.max(1, height),
          dp(Math.max(1, width)), dp(Math.max(1, height)));
    }
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    goOffscreen();
  }

  // --- Native (cpp-adapter.cpp) ----------------------------------------------

  private native boolean nAttach(
      int contextId, Surface surface, int pxW, int pxH, int dpW, int dpH, boolean opaque);

  private native void nResize(int contextId, int pxW, int pxH, int dpW, int dpH);

  private native void nSetOpaque(int contextId, boolean opaque);

  private native void nDetach(int contextId);
}
