package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;

@SuppressLint("ViewConstructor")
public class WebGPUTextureView extends TextureView implements TextureView.SurfaceTextureListener {

  WebGPUAPI mApi;
  private Surface mSurface;

  public WebGPUTextureView(Context context, WebGPUAPI api, boolean opaque) {
    super(context);
    mApi = api;
    setOpaque(opaque);
    setSurfaceTextureListener(this);
  }

  // Android 10 draws an opaque TextureView with SkBlendMode::kSrc, and under
  // a rotated, rounded clip its Skia (m75) can clear the stencil the clip was
  // written in before that draw, which leaves the canvas blank (fixed in
  // Skia m77, https://skia-review.googlesource.com/c/skia/+/223970). The
  // view blends on that release, whatever the GL: a canvas that covers its
  // bounds draws the same pixels.
  @Override
  public void setOpaque(boolean opaque) {
    super.setOpaque(opaque && Build.VERSION.SDK_INT != Build.VERSION_CODES.Q);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mSurface = new Surface(surfaceTexture);
    mApi.surfaceCreated(mSurface);
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mApi.surfaceChanged(mSurface);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    // Detach first (synchronous through JNI) so the native side has dropped
    // its window reference before we release ours.
    mApi.surfaceOffscreen();
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // No implementation needed
  }
}
