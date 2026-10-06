package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

import java.util.Objects;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {

  // Backing view kinds, see resolveKind().
  // Swapchain mode: the rendering thread presents the view's surface itself.
  private static final int KIND_SURFACE_VIEW = 0;
  private static final int KIND_TEXTURE_VIEW = 1;
  // Canvas mode: finished frames are put on screen from the UI thread.
  private static final int KIND_HARDWARE_BUFFER_VIEW = 2;
  private static final int KIND_BLIT_TEXTURE_VIEW = 3;
  private static final int KIND_BLIT_SURFACE_VIEW = 4;

  private int mContextId;
  private boolean mOpaque = true;
  private String mSurfaceType = "auto";
  private boolean mZOrderOnTop = false;
  private boolean mSwapchainMode = false;
  private int mAppliedKind = -1;
  private boolean mAppliedZOrderOnTop;
  // The hardware buffer view reported that it cannot present this canvas (see
  // hardwareBufferUnavailable()). Sticky until the surface type prop changes.
  private boolean mHardwareBufferUnavailable;
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
    if (!Objects.equals(value, mSurfaceType)) {
      mHardwareBufferUnavailable = false;
    }
    mSurfaceType = value;
  }

  public void setZOrderOnTop(boolean value) {
    mZOrderOnTop = value;
  }

  public void setMode(String value) {
    mSwapchainMode = "swapchain".equals(value);
  }

  // Resolve the backing view from the props. surfaceType picks how the canvas
  // composites: a SurfaceView is its own compositor layer, a TextureView is
  // regular view content; "auto" takes SurfaceView for an opaque canvas and
  // TextureView otherwise. The mode picks who presents: in canvas mode (the
  // default) the UI thread copies each finished frame onto the view's surface
  // (WebGPUBlitSurfaceView / WebGPUBlitTextureView), in swapchain mode the
  // rendering thread presents the surface itself (WebGPUSurfaceView /
  // WebGPUTextureView).
  //
  // WebGPUHardwareBufferView (a plain View drawing AHardwareBuffers inline,
  // API 29+) is a canvas-mode view with no swapchain, opt-in via surfaceType.
  // When hardware buffers turn out not to work for the canvas (device, format
  // or usage), it is replaced at runtime by WebGPUBlitTextureView; below API
  // 29, or in swapchain mode, the request resolves to a TextureView, which
  // composites the same way.
  private int resolveKind() {
    boolean surfaceView;
    if ("SurfaceView".equals(mSurfaceType)) {
      surfaceView = true;
    } else if ("TextureView".equals(mSurfaceType)) {
      surfaceView = false;
    } else if ("HardwareBufferView".equals(mSurfaceType)) {
      if (!mSwapchainMode && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        return mHardwareBufferUnavailable ? KIND_BLIT_TEXTURE_VIEW : KIND_HARDWARE_BUFFER_VIEW;
      }
      surfaceView = false;
    } else {
      surfaceView = mOpaque;
    }
    if (mSwapchainMode) {
      return surfaceView ? KIND_SURFACE_VIEW : KIND_TEXTURE_VIEW;
    }
    return surfaceView ? KIND_BLIT_SURFACE_VIEW : KIND_BLIT_TEXTURE_VIEW;
  }

  private static boolean isSurfaceView(int kind) {
    return kind == KIND_SURFACE_VIEW || kind == KIND_BLIT_SURFACE_VIEW;
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. Only a change of backing view (or of zOrderOnTop,
  // which a SurfaceView must know before it attaches) replaces the child;
  // opacity is applied in place.
  public void updateView() {
    int kind = resolveKind();
    boolean zOrderOnTop = isSurfaceView(kind) && mZOrderOnTop;
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
        case KIND_BLIT_TEXTURE_VIEW:
          mView = new WebGPUBlitTextureView(ctx, this, mOpaque);
          break;
        case KIND_BLIT_SURFACE_VIEW:
          mView = new WebGPUBlitSurfaceView(ctx, this, zOrderOnTop, mOpaque);
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
    } else if (kind == KIND_BLIT_TEXTURE_VIEW) {
      ((WebGPUBlitTextureView) mView).setOpaque(mOpaque);
    } else if (kind == KIND_BLIT_SURFACE_VIEW) {
      ((WebGPUBlitSurfaceView) mView).setOpaque(mOpaque);
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
  public void hardwareBufferUnavailable() {
    if (mHardwareBufferUnavailable) {
      return;
    }
    mHardwareBufferUnavailable = true;
    updateView();
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
