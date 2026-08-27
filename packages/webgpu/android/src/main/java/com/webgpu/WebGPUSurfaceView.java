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

  public WebGPUSurfaceView(Context context, WebGPUAPI api) {
    this(context, api, false);
  }

  public WebGPUSurfaceView(Context context, WebGPUAPI api, boolean transparent) {
    super(context);
    mApi = api;
    if (transparent) {
      // Own SurfaceFlinger layer, so frames skip the app's HWUI renderer. The
      // cost is that this layer sits above every other view in the window.
      setZOrderOnTop(true);
      getHolder().setFormat(PixelFormat.TRANSLUCENT);
    }
    getHolder().addCallback(this);
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
