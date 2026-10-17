package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {
  private static final String TAG = "WebGPUView";

  // Backing view kinds, see updateView().
  private static final int KIND_SURFACE_VIEW = 0;
  private static final int KIND_TEXTURE_VIEW = 1;
  private static final int KIND_HARDWARE_BUFFER_VIEW = 2;

  private int mContextId;
  // Props. Only opaque is applied in place; the others are read once, when
  // the backing view is created (see updateView()).
  private boolean mOpaque = true;
  private String mSurfaceType = "auto";
  private boolean mZOrderOnTop = false;
  // The kind the backing view was created with; -1 before it exists.
  private int mKind = -1;
  private WebGPUModule mModule;
  // The backing view, created by the first prop transaction and kept.
  private View mView = null;
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
    if (ignoredOnceMounted("android.surfaceType")) {
      return;
    }
    mSurfaceType = value;
  }

  public void setZOrderOnTop(boolean value) {
    if (ignoredOnceMounted("android.zOrderOnTop")) {
      return;
    }
    mZOrderOnTop = value;
  }

  // The backing view is chosen from the props of the first transaction and
  // kept (see updateView()). React calls a setter again only when its prop
  // changed, so a call after that is a change, which is ignored: a canvas
  // that needs another backing view is remounted (given a new key).
  private boolean ignoredOnceMounted(String prop) {
    if (mView == null) {
      return false;
    }
    Log.w(TAG, prop + " changed on a mounted canvas and is ignored: the backing view"
        + " is chosen when the canvas mounts. Remount the canvas (give it a new key)"
        + " to change it.");
    return true;
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
  // options have arrived. The first one creates the backing view, which is
  // then kept: a SurfaceView must know zOrderOnTop before it attaches, and
  // replacing a backing view on screen hands the canvas from one view to
  // another, which no release of Android does without a frame of nothing, or
  // of black, in between (the two views are composited by different frames).
  // A canvas that needs another backing view is remounted instead. Opacity is
  // applied in place; WebGPUHardwareBufferView always composites with alpha,
  // so there it is a no-op.
  public void updateView() {
    if (mView != null) {
      if (mKind == KIND_TEXTURE_VIEW) {
        ((WebGPUTextureView) mView).setOpaque(mOpaque);
      } else if (mKind == KIND_SURFACE_VIEW) {
        ((WebGPUSurfaceView) mView).setOpaque(mOpaque);
      }
      return;
    }
    int kind = resolveKind();
    boolean zOrderOnTop = kind == KIND_SURFACE_VIEW && mZOrderOnTop;
    mKind = kind;
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
    boolean votesFrameRate = mKind == KIND_SURFACE_VIEW;
    if (votesFrameRate) {
      mFrameRateVote.attach(surface);
    }
    onSurfaceCreate(surface, mContextId, width, height, votesFrameRate);
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
    // The surface is still valid here, so the vote can be taken back.
    mFrameRateVote.detach();
    switchToOffscreenSurface(mContextId);
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
    mFrameRateVote.detach();
    onViewDestroyed(mContextId);
  }

  // reportAnimation asks native to call onAnimating() when frames start being
  // presented to this surface back to back.
  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float width,
    float height,
    boolean reportAnimation
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

  // See SurfaceFrameRateVote.AnimationSource.
  @DoNotStrip
  private native long releaseFrameRateVoteIfIdle(int contextId, long idleAfterNanos);

}
