package com.webgpu;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;

public class WebGPUTextureView extends WebGPUBaseView implements TextureView.SurfaceTextureListener {

  public WebGPUTextureView(Context context) {
    super(context);
    TextureView textureView = new TextureView(context);
    textureView.setSurfaceTextureListener(this);
    textureView.setOpaque(false);
    addView(textureView);
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    Surface surface = new Surface(surfaceTexture);
    handleSurfaceCreate(surface, getWidth(), getHeight());
  }

  @Override
  public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    Surface surface = new Surface(surfaceTexture);
    handleSurfaceChanged(surface, width, height);
  }

  @Override
  public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
    handleSurfaceDestroyed();
    return true;
  }

  @Override
  public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    // No implementation needed
  }
}
