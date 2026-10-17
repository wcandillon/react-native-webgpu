package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

@SuppressLint("ViewConstructor")
public class WebGPUTextureView extends TextureView implements TextureView.SurfaceTextureListener, FrameKeeper {

  WebGPUAPI mApi;
  private Surface mSurface;
  // The SurfaceTexture, with the frame in it, the view keeps while it is out
  // of its window, until it is attached again or retired.
  @Nullable private SurfaceTexture mKeptSurfaceTexture;
  private boolean mLeavingWindow = false;
  private boolean mRetired = false;

  public WebGPUTextureView(Context context, WebGPUAPI api, boolean opaque) {
    super(context);
    mApi = api;
    setOpaque(opaque);
    setSurfaceTextureListener(this);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mSurface = new Surface(surfaceTexture);
    mApi.surfaceCreated(mSurface, 0);
  }

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();
    mLeavingWindow = false;
    SurfaceTexture kept = mKeptSurfaceTexture;
    if (kept != null) {
      mKeptSurfaceTexture = null;
      setSurfaceTexture(kept);
      mApi.surfaceResumed();
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    mLeavingWindow = true;
    super.onDetachedFromWindow();
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mApi.surfaceChanged(mSurface);
  }

  // Destroyed as the view leaves its window, the SurfaceTexture is kept, still
  // connected to the canvas, so it shows its last frame when the view comes
  // back. Destroyed while the view stays attached, as Android 7 does when the
  // window drops its hardware resources, or once the view is retired, it is
  // released.
  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    if (mLeavingWindow && !mRetired) {
      mApi.surfaceSuspended();
      mKeptSurfaceTexture = surfaceTexture;
      return false;
    }
    // Synchronous through JNI: the native side drops its window reference
    // before the view releases the surface.
    mApi.surfaceOffscreen();
    releaseSurface();
    return true;
  }

  @Override
  public void retire() {
    mRetired = true;
    SurfaceTexture kept = mKeptSurfaceTexture;
    if (kept == null) {
      return;
    }
    mKeptSurfaceTexture = null;
    mApi.surfaceOffscreen();
    kept.release();
    releaseSurface();
  }

  private void releaseSurface() {
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // No implementation needed
  }
}
