package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {


  private int mContextId;
  private boolean mTransparent = false;
  private String mAndroidSurfaceType = "auto";
  private boolean mZOrderOnTop = false;
  private boolean mTranslucent = false;
  private boolean mAppliedTextureView;
  private boolean mAppliedZOrderOnTop;
  private boolean mAppliedTranslucent;
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
    mTransparent = value;
  }

  public void setAndroidSurfaceType(String value) {
    mAndroidSurfaceType = value;
  }

  public void setZOrderOnTop(boolean value) {
    mZOrderOnTop = value;
  }

  public void setTranslucent(boolean value) {
    mTranslucent = value;
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. Avoid replacing the surface for unrelated updates.
  public void updateView() {
    boolean textureView = "TextureView".equals(mAndroidSurfaceType)
      || (!"SurfaceView".equals(mAndroidSurfaceType) && mTransparent);
    boolean zOrderOnTop = !textureView && mZOrderOnTop;
    boolean translucent = !textureView && mTranslucent;
    if (mView != null && textureView == mAppliedTextureView
        && zOrderOnTop == mAppliedZOrderOnTop && translucent == mAppliedTranslucent) {
      return;
    }
    if (mView != null) {
      removeView(mView);
    }
    mAppliedTextureView = textureView;
    mAppliedZOrderOnTop = zOrderOnTop;
    mAppliedTranslucent = translucent;
    Context ctx = getContext();
    mView = textureView
      ? new WebGPUTextureView(ctx, this)
      : new WebGPUSurfaceView(ctx, this, zOrderOnTop, translucent);
    addView(mView);
    // Fabric may not send another layout when only rendering props change.
    mView.layout(0, 0, getMeasuredWidth(), getMeasuredHeight());
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    if (mView != null) {
      mView.layout(0, 0, getMeasuredWidth(), getMeasuredHeight());
    }
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
