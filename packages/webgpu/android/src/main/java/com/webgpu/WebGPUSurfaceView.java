package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

public class WebGPUSurfaceView extends WebGPUBaseView implements SurfaceHolder.Callback {

  SurfaceView mSurfaceView;

  public WebGPUSurfaceView(Context context) {
    super(context);
    mSurfaceView = new SurfaceView(context);
    mSurfaceView.getHolder().addCallback(this);
    addView(mSurfaceView);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    mSurfaceView.layout(0, 0, this.getMeasuredWidth(), this.getMeasuredHeight());
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
