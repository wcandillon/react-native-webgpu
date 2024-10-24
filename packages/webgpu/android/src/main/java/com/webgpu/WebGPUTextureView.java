package com.webgpu;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import androidx.annotation.NonNull;

import org.w3c.dom.Text;

public class WebGPUTextureView extends WebGPUBaseView implements TextureView.SurfaceTextureListener {

  TextureView mTextureView;

  public WebGPUTextureView(Context context) {
    super(context);
    mTextureView = new TextureView(context);
    mTextureView.setSurfaceTextureListener(this);
    mTextureView.setOpaque(false);
    addView(mTextureView);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    mTextureView.layout(0, 0, this.getMeasuredWidth(), this.getMeasuredHeight());
  }

  @Override
  public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    Surface surface = new Surface(surfaceTexture);
    handleSurfaceCreate(surface, width, height);
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
