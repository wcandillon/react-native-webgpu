package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {


  private int mContextId;
  private boolean mOpaque = true;
  private String mSurfaceType = "auto";
  private boolean mZOrderOnTop = false;
  private boolean mAppliedTextureView;
  private boolean mAppliedZOrderOnTop;
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

  public void setOpaque(boolean value) {
    mOpaque = value;
  }

  public void setSurfaceType(String value) {
    mSurfaceType = value;
  }

  public void setZOrderOnTop(boolean value) {
    mZOrderOnTop = value;
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. Only a change of backing view (or of zOrderOnTop,
  // which a SurfaceView must know before it attaches) replaces the child;
  // opacity is applied in place.
  public void updateView() {
    boolean textureView = "TextureView".equals(mSurfaceType)
      || (!"SurfaceView".equals(mSurfaceType) && !mOpaque);
    boolean zOrderOnTop = !textureView && mZOrderOnTop;
    if (mView == null || textureView != mAppliedTextureView
        || zOrderOnTop != mAppliedZOrderOnTop) {
      if (mView != null) {
        removeView(mView);
      }
      mAppliedTextureView = textureView;
      mAppliedZOrderOnTop = zOrderOnTop;
      Context ctx = getContext();
      mView = textureView
        ? new WebGPUTextureView(ctx, this, mOpaque)
        : new WebGPUSurfaceView(ctx, this, zOrderOnTop, mOpaque);
      addView(mView);
      // ReactViewGroup.requestLayout() is a deliberate no-op, so addView outside
      // the layout pass never lays the child out; do it by hand or the new view
      // stays 0x0 and never gets a surface.
      layoutChild();
    } else if (textureView) {
      ((WebGPUTextureView) mView).setOpaque(mOpaque);
    } else {
      ((WebGPUSurfaceView) mView).setOpaque(mOpaque);
    }
  }

  private void layoutChild() {
    mView.layout(0, 0, getMeasuredWidth(), getMeasuredHeight());
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    if (mView != null) {
      layoutChild();
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
