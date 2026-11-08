package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.TextureView;
import androidx.annotation.NonNull;

@SuppressLint("ViewConstructor")
public class WebGPUTextureView extends TextureView implements TextureView.SurfaceTextureListener {

  WebGPUAPI mApi;

  public WebGPUTextureView(Context context, WebGPUAPI api, boolean opaque) {
    super(context);
    mApi = api;
    // An opaque TextureView lets the UI toolkit skip blending it; it can be
    // toggled on a live view.
    setOpaque(opaque);
    setSurfaceTextureListener(this);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mApi.onSurfaceTextureCreated(surfaceTexture, width, height);
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    mApi.onSurfaceTextureChanged(surfaceTexture, width, height);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    return mApi.onSurfaceTextureDestroyed(surfaceTexture);
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surface) {
    mApi.onSurfaceTextureUpdated();
  }
}
