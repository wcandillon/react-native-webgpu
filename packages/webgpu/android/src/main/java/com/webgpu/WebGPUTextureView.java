package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
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

  // Hands the view its texture before its first draw, which then shows the
  // frame presented into it (BackingViewKind.receivesSurfaceBeforeFirstDraw).
  // TextureView reports no onSurfaceTextureAvailable for a texture it was given.
  void supplySurfaceTexture(int width, int height) {
    SurfaceTexture surfaceTexture = new SurfaceTexture(false);
    surfaceTexture.setDefaultBufferSize(width, height);
    setSurfaceTexture(surfaceTexture);
    mSurface = new Surface(surfaceTexture);
    mApi.surfaceCreated(mSurface, 0, true);
  }

  // The canvas moved to another view: this one keeps its last frame on screen
  // until it is removed, and reports nothing.
  void detachFromRenderer() {
    mApi = WebGPUAPI.DETACHED;
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mSurface = new Surface(surfaceTexture);
    mApi.surfaceCreated(mSurface, 0, false);
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
    mApi.surfaceUpdated();
  }
}
