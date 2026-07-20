package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;

@SuppressLint("ViewConstructor")
public class WebGPUTextureView extends TextureView implements TextureView.SurfaceTextureListener {

  private final WebGPUAPI mApi;
  private Surface mSurface;

  public WebGPUTextureView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
    setOpaque(false);
    setSurfaceTextureListener(this);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    releaseSurface();
    mSurface = new Surface(surfaceTexture);
    mApi.surfaceCreated(this, mSurface);
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    if (mSurface == null) {
      mSurface = new Surface(surfaceTexture);
    }
    mApi.surfaceChanged(this, mSurface);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    mApi.surfaceOffscreen(this);
    releaseSurface();
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // No implementation needed
  }

  @Override
  protected void onDetachedFromWindow() {
    mApi.surfaceOffscreen(this);
    releaseSurface();
    super.onDetachedFromWindow();
  }

  private void releaseSurface() {
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
  }
}
