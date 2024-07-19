package com.webgpu;

import androidx.annotation.NonNull;

import android.content.Context;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements SurfaceHolder.Callback {

  private final SurfaceView mSurfaceView;

  public WebGPUView(Context context) {
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
    connectSurface(holder.getSurface());
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {

  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {

  }

  @DoNotStrip
  private native void connectSurface(Surface surface);
}
