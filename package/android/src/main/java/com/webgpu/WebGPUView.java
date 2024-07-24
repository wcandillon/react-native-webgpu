package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.OptIn;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.uimanager.ThemedReactContext;

public class WebGPUView extends SurfaceView implements SurfaceHolder.Callback {

  private Integer mContextId;
  private @OptIn(markerClass = FrameworkAPI.class) WebGPUModule module;

  public WebGPUView(Context context) {
    super(context);
    getHolder().addCallback(this);
  }

  @OptIn(markerClass = FrameworkAPI.class)
  public void setContextId(Integer contextId) {
    if (module == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        module = ((ThemedReactContext)context).getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
  }

  @Override
  @OptIn(markerClass = FrameworkAPI.class)
  public void surfaceCreated(@NonNull SurfaceHolder holder) {
    onSurfaceCreate(holder.getSurface(), mContextId, this.getMeasuredWidth(), this.getMeasuredHeight());
    if (module != null) {
      module.onSurfaceCreated(mContextId);
    }
  }

  @Override
  public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {}

  @Override
  public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
    onSurfaceDestroy(mContextId);
  }

  @DoNotStrip
  private native void onSurfaceCreate(Surface surface, int contextId, int width, int height);

  @DoNotStrip
  private native void onSurfaceDestroy(int contextId);
}
