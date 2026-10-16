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
    boolean votesFrameRate = mAppliedKind == KIND_SURFACE_VIEW;
    if (votesFrameRate) {
      mFrameRateVote.attach(surface);
    }
    onSurfaceCreate(surface, mContextId, width, height, presentsBehindPendingDraw(),
        acquiresAfterDraw(), votesFrameRate);
  }

  // A TextureView shows a new frame only when its window draws. On Android 11
  // the frame is acquired only after that draw (acquiresAfterDraw), which keeps
  // the held present clear of the loader's 4.3 s dequeue timeout.
  private boolean presentsBehindPendingDraw() {
    return mAppliedKind == KIND_TEXTURE_VIEW;
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
    if (!acquiresAfterDraw()) {
      return;
    }
    long presentCount = onSurfaceDrawing(mContextId);
    // The window takes the frame when this draw syncs with its render thread,
    // and a message posted now runs after that sync.
    mMainHandler.post(() -> onSurfaceDrawn(mContextId, presentCount));
  }

  // Android 11's Vulkan loader hands a TextureView's buffer queue its dequeue
  // timeout as an int where the window reads an int64_t (frameworks/native
  // android-11.0.0_r48, vulkan/libvulkan/swapchain.cpp), so the queue waits
  // up to 4.3 s for a free buffer: a frame is acquired only once the window
  // has drawn the one presented last.
  private boolean acquiresAfterDraw() {
    return mAppliedKind == KIND_TEXTURE_VIEW && Build.VERSION.SDK_INT == Build.VERSION_CODES.R;
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
