package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

public class WebGPUSurfaceView extends WebGPUBaseView implements SurfaceHolder.Callback {

  public WebGPUSurfaceView(Context context) {
    super(context);
    SurfaceView surfaceView = new SurfaceView(context);
    surfaceView.getHolder().addCallback(this);
    addView(surfaceView);
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    handleSurfaceCreate(holder.getSurface(), getWidth(), getHeight());
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    handleSurfaceChanged(holder.getSurface(), getWidth(), getHeight());
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    handleSurfaceDestroyed();
  }
}
