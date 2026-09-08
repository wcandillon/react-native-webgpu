package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PixelFormat;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

@SuppressLint("ViewConstructor")
public class WebGPUSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

  WebGPUAPI mApi;

  public WebGPUSurfaceView(Context context, WebGPUAPI api, boolean zOrderOnTop, boolean opaque) {
    super(context);
    mApi = api;
    setZOrderOnTop(zOrderOnTop);
    setOpaque(opaque);
    getHolder().addCallback(this);
  }

  // The format drives the compositor's opaque flag for this layer. It can
  // change on a live surface: SurfaceView reports it through surfaceChanged.
  public void setOpaque(boolean opaque) {
    getHolder().setFormat(opaque ? PixelFormat.OPAQUE : PixelFormat.TRANSLUCENT);
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    // surfaceDestroyed() normally fires during detach as well; going offscreen
    // is idempotent, so this is just a safety net for paths where it does not.
    mApi.surfaceOffscreen();
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    mApi.surfaceCreated(holder.getSurface());
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    mApi.surfaceChanged(holder.getSurface());
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    mApi.surfaceOffscreen();
  }
}
