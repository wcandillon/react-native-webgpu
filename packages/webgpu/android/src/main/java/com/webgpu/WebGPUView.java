package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {

  // Backing view kinds, see updateView().
  private static final int KIND_SURFACE_VIEW = 0;
  private static final int KIND_TEXTURE_VIEW = 1;
  private static final int KIND_HARDWARE_BUFFER_VIEW = 2;

  private int mContextId;
  private boolean mOpaque = true;
  private String mSurfaceType = "auto";
  private boolean mZOrderOnTop = false;
  private int mAppliedKind = -1;
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

  // Resolve the backing view from the props. "auto" picks SurfaceView for an
  // opaque canvas and, for a non-opaque one, WebGPUHardwareBufferView (a plain
  // View drawing AHardwareBuffers inline, API 29+) or TextureView below that.
  private int resolveKind() {
    if ("SurfaceView".equals(mSurfaceType)) {
      return KIND_SURFACE_VIEW;
    }
    if ("TextureView".equals(mSurfaceType)) {
      return KIND_TEXTURE_VIEW;
    }
    boolean hardwareBufferSupported = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    if ("HardwareBufferView".equals(mSurfaceType)) {
      return hardwareBufferSupported ? KIND_HARDWARE_BUFFER_VIEW : KIND_TEXTURE_VIEW;
    }
    if (mOpaque) {
      return KIND_SURFACE_VIEW;
    }
    return hardwareBufferSupported ? KIND_HARDWARE_BUFFER_VIEW : KIND_TEXTURE_VIEW;
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. Only a change of backing view (or of zOrderOnTop,
  // which a SurfaceView must know before it attaches) replaces the child;
  // opacity is applied in place.
  public void updateView() {
    int kind = resolveKind();
    boolean zOrderOnTop = kind == KIND_SURFACE_VIEW && mZOrderOnTop;
    if (mView == null || kind != mAppliedKind || zOrderOnTop != mAppliedZOrderOnTop) {
      if (mView != null) {
        removeView(mView);
      }
      mAppliedKind = kind;
      mAppliedZOrderOnTop = zOrderOnTop;
      Context ctx = getContext();
      switch (kind) {
        case KIND_HARDWARE_BUFFER_VIEW:
          mView = new WebGPUHardwareBufferView(ctx, this);
          break;
        case KIND_TEXTURE_VIEW:
          mView = new WebGPUTextureView(ctx, this, mOpaque);
          break;
        default:
          mView = new WebGPUSurfaceView(ctx, this, zOrderOnTop, mOpaque);
          break;
      }
      addView(mView);
      // ReactViewGroup.requestLayout() is a deliberate no-op, so addView outside
      // the layout pass never lays the child out; do it by hand or the new view
      // stays 0x0 and never gets a surface.
      layoutChild();
    } else if (kind == KIND_TEXTURE_VIEW) {
      ((WebGPUTextureView) mView).setOpaque(mOpaque);
    } else if (kind == KIND_SURFACE_VIEW) {
      ((WebGPUSurfaceView) mView).setOpaque(mOpaque);
    }
    // WebGPUHardwareBufferView always composites with alpha; opacity is a no-op.
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

  @Override
  public int getContextId() {
    return mContextId;
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
