package com.webgpu;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

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
  // Once React drops the view its registry entry is retired: what its backing
  // view reports afterwards, until it leaves the window, attaches no surface
  // and replaces no view.
  private boolean mDestroyed = false;
  // Only a SurfaceView votes: a TextureView or HardwareBufferView is
  // composited into the window, which votes for it.
  private final SurfaceFrameRateVote mFrameRateVote = new SurfaceFrameRateVote(
    this, idleAfterNanos -> releaseFrameRateVoteIfIdle(mContextId, idleAfterNanos));

  WebGPUView(Context context) {
    super(context);
    // ReactViewGroup disables clipChildren (RN overflow is handled with canvas
    // clipping instead). Android copies the parent's clipChildren into each
    // child's RenderNode.clipToBounds, and HWUI treats a damaged node without
    // it as unbounded damage (RenderNode::damageSelf). The TextureView and
    // HardwareBufferView children invalidate on every frame, so without this
    // every non-opaque canvas frame repaints the whole window. Our only child
    // exactly fills this view, so clipping it changes nothing visually.
    setClipChildren(true);
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
  // opaque canvas and TextureView for a non-opaque one. WebGPUHardwareBufferView
  // (a plain View drawing AHardwareBuffers inline, API 29+) is opt-in via
  // surfaceType until it gets a runtime fallback to TextureView when the
  // AHardwareBuffer import fails on a device; below API 29 the request itself
  // falls back to TextureView.
  private int resolveKind() {
    if ("SurfaceView".equals(mSurfaceType)) {
      return KIND_SURFACE_VIEW;
    }
    if ("TextureView".equals(mSurfaceType)) {
      return KIND_TEXTURE_VIEW;
    }
    if ("HardwareBufferView".equals(mSurfaceType)) {
      boolean hardwareBufferSupported = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
      return hardwareBufferSupported ? KIND_HARDWARE_BUFFER_VIEW : KIND_TEXTURE_VIEW;
    }
    return mOpaque ? KIND_SURFACE_VIEW : KIND_TEXTURE_VIEW;
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. Only a change of backing view (or of zOrderOnTop,
  // which a SurfaceView must know before it attaches) replaces the child;
  // opacity is applied in place.
  public void updateView() {
    int kind = resolveKind();
    boolean zOrderOnTop = kind == KIND_SURFACE_VIEW && mZOrderOnTop;
    if (mView == null || kind != mAppliedKind || zOrderOnTop != mAppliedZOrderOnTop) {
      if (mView instanceof WebGPUTextureView) {
        keepTextureViewFrame((WebGPUTextureView) mView);
      }
      if (mView != null) {
        removeView(mView);
        retire(mView);
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
  public void surfaceCreated(Surface surface, long firstFrameToken) {
    if (mDestroyed) {
      return;
    }
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    boolean votesFrameRate = mAppliedKind == KIND_SURFACE_VIEW;
    if (votesFrameRate) {
      mFrameRateVote.attach(surface);
    }
    onSurfaceCreate(surface, mContextId, width, height, votesFrameRate, firstFrameToken);
  }

  /**
   * The first frame was presented into the surface firstFrameToken names.
   * Called from native on the thread that presented it.
   */
  @DoNotStrip
  public void onFirstFramePresented(long firstFrameToken) {
    post(() -> {
      if (mView instanceof WebGPUSurfaceView) {
        ((WebGPUSurfaceView) mView).onFirstFramePresented(firstFrameToken);
      }
    });
  }

  @Override
  public void surfaceChanged(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    onSurfaceChanged(surface, mContextId, width, height);
  }

  @Override
  public void keepFrame(Surface surface) {
    long size = keepableFrameSize(mContextId);
    Bitmap frame = SurfaceFrameCapture.capture(surface, (int) (size >>> 32), (int) size);
    keepCapturedFrame(frame);
  }

  // A TextureView replaced by another backing view takes its frame with it;
  // its layer, which getBitmap reads, is gone once it leaves the window.
  private void keepTextureViewFrame(WebGPUTextureView view) {
    long size = keepableFrameSize(mContextId);
    int width = (int) (size >>> 32);
    int height = (int) size;
    if (width > 0 && height > 0 && view.isAvailable()) {
      keepCapturedFrame(view.getBitmap(width, height));
    }
  }

  private void keepCapturedFrame(@Nullable Bitmap frame) {
    if (frame != null) {
      keepBitmapFrame(mContextId, frame);
      frame.recycle();
    }
  }

  @Override
  public void surfaceOffscreen() {
    // The surface is still valid here, so the vote can be taken back.
    mFrameRateVote.detach();
    switchToOffscreenSurface(mContextId);
  }

  @Override
  public void surfaceSuspended() {
    suspendSurface(mContextId);
  }

  @Override
  public void surfaceResumed() {
    resumeSurface(mContextId);
  }

  /**
   * Called from native (AnimationReporter in cpp-adapter.cpp) on the thread
   * that presents to the surface, once per animation, only for a SurfaceView.
   */
  @DoNotStrip
  void onAnimating() {
    mFrameRateVote.onAnimating();
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
    mDestroyed = true;
    if (mView != null) {
      retire(mView);
    }
    mFrameRateVote.detach();
    onViewDestroyed(mContextId);
  }

  private static void retire(View view) {
    if (view instanceof FrameKeeper) {
      ((FrameKeeper) view).retire();
    }
  }

  // reportAnimation asks native to call onAnimating() when frames start being
  // presented to this surface back to back.
  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float width,
    float height,
    boolean reportAnimation,
    long firstFrameToken
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
  private native void suspendSurface(int contextId);

  @DoNotStrip
  private native void resumeSurface(int contextId);

  // The drawing buffer's size a kept frame is read at, width << 32 | height;
  // 0 when there is nothing to keep.
  @DoNotStrip
  private native long keepableFrameSize(int contextId);

  @DoNotStrip
  private native boolean keepBitmapFrame(int contextId, Bitmap frame);

  @DoNotStrip
  private native void onViewDestroyed(int contextId);

  // See SurfaceFrameRateVote.AnimationSource.
  @DoNotStrip
  private native long releaseFrameRateVoteIfIdle(int contextId, long idleAfterNanos);

}
