package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;

import androidx.annotation.NonNull;

/**
 * The canvas-mode view for a canvas composited like a regular view: a TextureView whose surface is
 * only used from the UI thread, where the latest finished frame is copied onto it and presented
 * behind the window's pending draw (see {@link BlitPresenterClient} and {@link FrameScheduler}). It
 * is the default for a non-opaque canvas, and what a {@link WebGPUHardwareBufferView} is replaced
 * with when hardware buffers cannot back the canvas.
 */
@SuppressLint("ViewConstructor")
public class WebGPUBlitTextureView extends TextureView
    implements TextureView.SurfaceTextureListener {

  private final BlitPresenterClient mClient;
  private Surface mSurface;

  public WebGPUBlitTextureView(Context context, WebGPUAPI api, boolean opaque) {
    super(context);
    mClient = new BlitPresenterClient(api, FrameScheduler.Kind.TEXTURE_VIEW);
    setOpaque(opaque);
    setSurfaceTextureListener(this);
  }

  private int toDp(int px) {
    final float density = getResources().getDisplayMetrics().density;
    return Math.max(1, Math.round(px / density));
  }

  @Override
  public void onSurfaceTextureAvailable(
      @NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mSurface = new Surface(surfaceTexture);
    mClient.surfaceAvailable(mSurface, toDp(width), toDp(height));
  }

  @Override
  public void onSurfaceTextureSizeChanged(
      @NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mClient.surfaceSizeChanged(toDp(width), toDp(height));
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    // Detach first (synchronous through JNI) so the native side has dropped its window reference
    // before we release ours.
    mClient.surfaceDestroyed();
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // HWUI latched the presented buffer during this draw pass; the next present goes behind it.
    mClient.frameLatched();
  }
}
