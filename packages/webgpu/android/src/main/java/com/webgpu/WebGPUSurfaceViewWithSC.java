package com.webgpu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.view.Surface;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;


@SuppressLint("ViewConstructor")
@RequiresApi(api = Build.VERSION_CODES.Q)
public class WebGPUSurfaceViewWithSC extends SurfaceView implements SurfaceHolder.Callback {

  WebGPUAPI mApi;
  SurfaceControl mSurfaceControl;
  Surface mSurface;

  public WebGPUSurfaceViewWithSC(Context context, WebGPUAPI api) {
    super(context);
    mApi = api;
    getHolder().addCallback(this);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);

  }

  @Override
  protected void onDetachedFromWindow() {
    mApi.surfaceOffscreen(this);
    if (mSurface != null) {
      mSurface.release();
      mSurface = null;
    }
    if (mSurfaceControl != null) {
      mSurfaceControl.release();
      mSurfaceControl = null;
    }
    super.onDetachedFromWindow();
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    if (mSurfaceControl != null) {
      SurfaceControl.Transaction tr = new SurfaceControl.Transaction();
      tr.setVisibility(mSurfaceControl, true);
      tr.reparent(mSurfaceControl, getSurfaceControl());
      tr.apply();
    } else {
      SurfaceControl.Builder scb = new SurfaceControl.Builder();
      scb.setName("WebGPUView");
      scb.setOpaque(true);
      scb.setBufferSize(getWidth(), getHeight());
      scb.setParent(getSurfaceControl());
      scb.setFormat(PixelFormat.RGBA_8888);
      mSurfaceControl = scb.build();
      mSurface = new Surface(mSurfaceControl);
    }
    // surfaceDestroyed() moves the existing SurfaceControl offscreen without
    // releasing it. Re-publish both new and reused surfaces here.
    mApi.surfaceCreated(this, mSurface);
    SurfaceControl.Transaction tr = new SurfaceControl.Transaction();
    tr.setVisibility(mSurfaceControl, true);
    tr.apply();
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    if (mSurface == null || mSurfaceControl == null) {
      return;
    }
    mApi.surfaceChanged(this, mSurface);
    SurfaceControl.Transaction tr = new SurfaceControl.Transaction();
    tr.setVisibility(mSurfaceControl, true);
    tr.setBufferSize(mSurfaceControl, getWidth(), getHeight());
    tr.apply();
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    mApi.surfaceOffscreen(this);
    if (mSurfaceControl == null) {
      return;
    }
    SurfaceControl.Transaction tr = new SurfaceControl.Transaction();
    tr.reparent(mSurfaceControl, null);
    tr.apply();
  }
}
