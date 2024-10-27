package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;

import org.w3c.dom.Text;

@SuppressLint("ViewConstructor")
public class WebGPUTextureView extends TextureView implements TextureView.SurfaceTextureListener {

  WebGPUAPI mApi;

  public WebGPUTextureView(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
    setOpaque(false);
    setSurfaceTextureListener(this);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    Surface surface = new Surface(surfaceTexture);
    mApi.surfaceCreated(surface);
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    Surface surface = new Surface(surfaceTexture);
    mApi.surfaceChanged(surface);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    mApi.surfaceDestroyed();
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // No implementation needed
  }
}
