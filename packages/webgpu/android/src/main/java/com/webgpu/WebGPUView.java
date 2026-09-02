package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {


  private int mContextId;
  private boolean mTransparent = false;
  private boolean mTransparentSurfaceLayer = false;
  private WebGPUModule mModule;
  private View mView = null;

  WebGPUView(Context context) {
    super(context);
  }

  public void setContextId(int contextId) {
    if (mModule == null) {
      Context context = getContext();
      if (context instanceof ThemedReactContext) {
        mModule = ((ThemedReactContext) context).getReactApplicationContext().getNativeModule(WebGPUModule.class);
      }
    }
    mContextId = contextId;
  }

  public void setTransparent(boolean value) {
    if (value == mTransparent && mView != null) {
      return;
    }
    mTransparent = value;
    rebuildView();
  }

  // "surface-overlay" trades z-ordering for a cheaper composition path; see
  // the androidTransparencyMode prop on Canvas. Ignored when not transparent.
  public void setTransparencyMode(String mode) {
    boolean surfaceLayer = "surface-overlay".equals(mode);
    if (surfaceLayer == mTransparentSurfaceLayer && mView != null) {
      return;
    }
    mTransparentSurfaceLayer = surfaceLayer;
    rebuildView();
  }

  private void rebuildView() {
    Context ctx = getContext();
    if (mView != null) {
      removeView(mView);
    }
    if (mTransparent) {
      mView = mTransparentSurfaceLayer
        ? new WebGPUSurfaceView(ctx, this, true)
        : new WebGPUTextureView(ctx, this);
    } else {
      mView = new WebGPUSurfaceView(ctx, this);
    }
    addView(mView);
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    mView.layout(0, 0, this.getMeasuredWidth(), this.getMeasuredHeight());
  }

  @Override
  public void surfaceCreated(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    onSurfaceCreate(surface, mContextId, width, height);
  }

  @Override
  public void surfaceChanged(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    onSurfaceChanged(surface, mContextId, width, height);
  }

  @Override
  public void surfaceOffscreen() {
    switchToOffscreenSurface(mContextId);
  }

  /**
   * Called from WebGPUViewManager.onDropViewInstance when React removes this
   * view: the view dies with its Canvas, so it retires the registry entry.
   */
  public void destroy() {
    onViewDestroyed(mContextId);
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
  private native void switchToOffscreenSurface(int contextId);

  @DoNotStrip
  private native void onViewDestroyed(int contextId);

}
