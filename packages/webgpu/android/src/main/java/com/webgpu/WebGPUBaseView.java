package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.View;
import com.facebook.proguard.annotations.DoNotStrip;

public abstract class WebGPUBaseView extends View {
  protected Integer mContextId;

  public WebGPUBaseView(Context context, Integer contextId) {
    super(context);
    mContextId = contextId;
  }

  protected void handleSurfaceCreate(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float scaledWidth = getWidth() / density;
    float scaledHeight = getHeight() / density;
    onSurfaceCreate(surface, mContextId, scaledWidth, scaledHeight);
  }

  protected void handleSurfaceChanged(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float scaledWidth = getWidth() / density;
    float scaledHeight = getHeight() / density;
    onSurfaceChanged(surface, mContextId, scaledWidth, scaledHeight);
  }

  protected void handleSurfaceDestroyed() {
    onSurfaceDestroy(mContextId);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
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
  protected native void switchToOffscreenSurface(int contextId);
}
