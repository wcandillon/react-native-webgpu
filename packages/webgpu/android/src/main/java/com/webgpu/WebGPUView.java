package com.webgpu;

import androidx.annotation.NonNull;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.facebook.proguard.annotations.DoNotStrip;

public class WebGPUView extends SurfaceView implements SurfaceHolder.Callback {

  private Integer mContextId;

  public WebGPUView(Context context) {
    super(context);
    getHolder().addCallback(this);
  }

  public void setContextId(Integer contextId) {
    mContextId = contextId;
  }

  @Override
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    float width = applyDensity(getWidth());
    float height = applyDensity(getHeight());
    onSurfaceCreate(holder.getSurface(), mContextId, width, height);
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
    float scaledWidth = applyDensity(width);
    float scaledHeight = applyDensity(height);
    onSurfaceChanged(holder.getSurface(), mContextId, scaledWidth, scaledHeight);
  }

  float applyDensity(float size) {
    float density = getResources().getDisplayMetrics().density;
    return size / density;
  }

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    onSurfaceDestroy(mContextId);
  }

  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float width,
    float height
  );
  @DoNotStrip
  private native void onSurfaceChanged(
    Surface surface,
    int contextId,
    float width,
    float height
  );

  @DoNotStrip
  private native void onSurfaceDestroy(int contextId);

}
