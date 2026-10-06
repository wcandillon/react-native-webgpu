package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PixelFormat;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

/**
 * The canvas-mode view for a canvas that is its own compositor layer: a SurfaceView whose surface
 * is only used from the UI thread, where the latest finished frame is copied onto it and presented
 * on the next vsync (see {@link BlitPresenterClient} and {@link FrameScheduler}). It is the default
 * for an opaque canvas.
 */
@SuppressLint("ViewConstructor")
public class WebGPUBlitSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

  private final BlitPresenterClient mClient;

  public WebGPUBlitSurfaceView(
      Context context, WebGPUAPI api, boolean zOrderOnTop, boolean opaque) {
    super(context);
    mClient = new BlitPresenterClient(api, FrameScheduler.Kind.SURFACE_VIEW);
    setZOrderOnTop(zOrderOnTop);
    setOpaque(opaque);
    getHolder().addCallback(this);
  }

  // The format drives the compositor's opaque flag for this layer. It can change on a live
  // surface: SurfaceView reports it through surfaceChanged.
  public void setOpaque(boolean opaque) {
    getHolder().setFormat(opaque ? PixelFormat.OPAQUE : PixelFormat.TRANSLUCENT);
  }

  private int toDp(int px) {
    final float density = getResources().getDisplayMetrics().density;
    return Math.max(1, Math.round(px / density));
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    // surfaceDestroyed() normally fires during detach as well; the client's detach is idempotent,
    // so this is just a safety net for paths where it does not.
    mClient.surfaceDestroyed();
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    mClient.surfaceAvailable(holder.getSurface(), toDp(getWidth()), toDp(getHeight()));
  }

  @Override
  public void surfaceChanged(
      @NonNull SurfaceHolder holder, int format, int width, int height) {
    mClient.surfaceSizeChanged(toDp(width), toDp(height));
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    mClient.surfaceDestroyed();
  }
}
