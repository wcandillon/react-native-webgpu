package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.View;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public abstract class WebGPUBaseView extends ReactViewGroup {
  protected Integer mContextId;
  protected WebGPUModule mModule;

  public WebGPUBaseView(Context context) {
    super(context);
  }

  public void setContextId(Integer contextId) {
    if (mModule == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        mModule = ((ThemedReactContext) context)
          .getReactApplicationContext()
          .getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
  }

  protected void handleSurfaceCreate(Surface surface, int width, int height) {
    float density = getResources().getDisplayMetrics().density;
    float scaledWidth = width / density;
    float scaledHeight = height / density;
    onSurfaceCreate(surface, mContextId, scaledWidth, scaledHeight);
  }

  protected void handleSurfaceChanged(Surface surface, int width, int height) {
    float density = getResources().getDisplayMetrics().density;
    float scaledWidth = width / density;
    float scaledHeight = height / density;
    onSurfaceChanged(surface, mContextId, scaledWidth, scaledHeight);
  }

  protected void handleSurfaceDestroyed() {
    onSurfaceDestroy(mContextId);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
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

  @DoNotStrip
  private native void switchToOffscreenSurface(int contextId);
}
