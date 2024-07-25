package com.webgpu;

import androidx.annotation.NonNull;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.PixelUtil;
import com.facebook.react.uimanager.ThemedReactContext;

public class WebGPUView extends SurfaceView implements SurfaceHolder.Callback {

  private Integer mContextId;
  private WebGPUModule mModule;

  public WebGPUView(Context context) {
    super(context);
    getHolder().addCallback(this);
  }

  public void setContextId(Integer contextId) {
    if (mModule == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        mModule = ((ThemedReactContext)context).getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    float clientWidth = getWidth();
    float clientHeight = getWidth();
    float width = PixelUtil.toDIPFromPixel(clientWidth);
    float height = PixelUtil.toDIPFromPixel(clientHeight);
    onSurfaceCreate(holder.getSurface(), mContextId, clientWidth, clientHeight, width, height);
    mModule.onSurfaceCreated(mContextId);
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {}

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    onSurfaceDestroy(mContextId);
  }

  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float clientWidth,
    float clientHeight,
    float width,
    float height
  );

  @DoNotStrip
  private native void onSurfaceDestroy(int contextId);
}
