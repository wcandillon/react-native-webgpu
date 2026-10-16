package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
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
  private BackingViewKind mAppliedKind = null;
  private boolean mAppliedZOrderOnTop;
  private WebGPUModule mModule;
  private View mView = null;
  // Native posts a frame presented on the main thread here, to present it
  // behind the draw the window has pending (see SurfaceRegistry.h).
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private final Runnable mPostedPresent = () -> presentPostedFrame(mContextId);
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

  // Resolve the backing view from the props: see BackingViewKind for what each
  // surfaceType, and "auto", picks.
  private BackingViewKind resolveKind() {
    BackingViewKind requested = BackingViewKind.fromSurfaceType(mSurfaceType, Build.VERSION.SDK_INT);
    if (requested != null) {
      return requested;
    }
    return BackingViewKind.forAutoSurfaceType(mOpaque, mZOrderOnTop, Build.VERSION.SDK_INT);
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. Only a change of backing view (or of zOrderOnTop,
  // which a SurfaceView must know before it attaches) replaces the child;
  // opacity is applied in place.
  public void updateView() {
    BackingViewKind kind = resolveKind();
    boolean zOrderOnTop = kind == BackingViewKind.SURFACE_VIEW && mZOrderOnTop;
    if (mView == null || kind != mAppliedKind || zOrderOnTop != mAppliedZOrderOnTop) {
      if (mView != null) {
        removeView(mView);
      }
      mAppliedKind = kind;
      mAppliedZOrderOnTop = zOrderOnTop;
      Context ctx = getContext();
      switch (kind) {
        case HARDWARE_BUFFER_VIEW:
          mView = new WebGPUHardwareBufferView(ctx, this);
          break;
        case TEXTURE_VIEW:
          mView = new WebGPUTextureView(ctx, this, mOpaque);
          break;
        case SURFACE_VIEW:
        default:
          mView = new WebGPUSurfaceView(ctx, this, zOrderOnTop, mOpaque);
          break;
      }
      addView(mView);
      // ReactViewGroup.requestLayout() is a deliberate no-op, so addView outside
      // the layout pass never lays the child out; do it by hand or the new view
      // stays 0x0 and never gets a surface.
      layoutChild();
    } else if (kind == BackingViewKind.TEXTURE_VIEW) {
      ((WebGPUTextureView) mView).setOpaque(mOpaque);
    } else if (kind == BackingViewKind.SURFACE_VIEW) {
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
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    boolean votesFrameRate = mAppliedKind == BackingViewKind.SURFACE_VIEW;
    if (votesFrameRate) {
      mFrameRateVote.attach(surface);
    }
    onSurfaceCreate(
      surface,
      mContextId,
      width,
      height,
      mAppliedKind.presentsBehindPendingDraw(),
      mAppliedKind.acquiresAfterDraw(Build.VERSION.SDK_INT),
      votesFrameRate,
      firstFrameToken);
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
  public void surfaceUpdated() {
    if (!mAppliedKind.acquiresAfterDraw(Build.VERSION.SDK_INT)) {
      return;
    }
    long presentCount = onSurfaceDrawing(mContextId);
    // The window takes the frame when this draw syncs with its render thread,
    // and a message posted now runs after that sync.
    mMainHandler.post(() -> onSurfaceDrawn(mContextId, presentCount));
  }

  @Override
  public int getContextId() {
    return mContextId;
  }

  /** Called by native on the main thread, see mPostedPresent. */
  @DoNotStrip
  private void postPresentBehindPendingDraw() {
    mMainHandler.post(mPostedPresent);
  }

  /**
   * Called from WebGPUViewManager.onDropViewInstance when React removes this
   * view: the view dies with its Canvas, so it retires the registry entry.
   */
  public void destroy() {
    mFrameRateVote.detach();
    onViewDestroyed(mContextId);
  }

  // presentsBehindPendingDraw has native post a present made on the main
  // thread through postPresentBehindPendingDraw(); acquiresAfterDraw has it
  // acquire a frame only once the window drew the last one (surfaceUpdated);
  // reportAnimation asks native to call onAnimating() when frames start being
  // presented to this surface back to back.
  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float width,
    float height,
    boolean presentsBehindPendingDraw,
    boolean acquiresAfterDraw,
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
  private native void presentPostedFrame(int contextId);

  @DoNotStrip
  private native long onSurfaceDrawing(int contextId);

  @DoNotStrip
  private native void onSurfaceDrawn(int contextId, long presentCount);

  @DoNotStrip
  private native void switchToOffscreenSurface(int contextId);

  @DoNotStrip
  private native void onViewDestroyed(int contextId);

  // See SurfaceFrameRateVote.AnimationSource.
  @DoNotStrip
  private native long releaseFrameRateVoteIfIdle(int contextId, long idleAfterNanos);

}
