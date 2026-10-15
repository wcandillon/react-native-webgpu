package com.webgpu;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

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
  // Attached and undrawn until the next swap, until the canvas leaves its
  // window or the window hides: see BackingViewKind.retiresWhenReplaced.
  private WebGPUSurfaceView mRetiredSurfaceView = null;
  // Above mView until mView shows the canvas, see updateView().
  private final OutgoingViewHold<WebGPUTextureView> mOutgoingView =
    new OutgoingViewHold<>(new OutgoingViewHold.Host<WebGPUTextureView>() {
      @Override
      public void removeView(WebGPUTextureView view) {
        WebGPUView.this.removeView(view);
      }

      @Override
      public void runAfterWindowFrameCommits(Runnable action) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
          action.run();
          return;
        }
        getViewTreeObserver().registerFrameCommitCallback(action);
        invalidate();
      }

      @Override
      public void postDelayed(Runnable action, long delayMillis) {
        WebGPUView.this.postDelayed(action, delayMillis);
      }

      @Override
      public void removeCallbacks(Runnable action) {
        WebGPUView.this.removeCallbacks(action);
      }
    });

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
      mOutgoingView.release();
      removeRetiredSurfaceView();
      if (mView != null) {
        if (mAppliedKind.retiresWhenReplaced(Build.VERSION.SDK_INT, isShown(), getWidth(), getHeight())) {
          retireSurfaceView((WebGPUSurfaceView) mView);
        } else if (kind.keepsOutgoingOnScreen(mAppliedKind, isShown(), getWidth(), getHeight())) {
          WebGPUTextureView outgoing = (WebGPUTextureView) mView;
          outgoing.detachFromRenderer();
          mOutgoingView.hold(outgoing, OutgoingViewHold.Arrival.ofSurfaceView(Build.VERSION.SDK_INT, zOrderOnTop));
        } else {
          if (mView instanceof WebGPUTextureView) {
            keepTextureViewFrame((WebGPUTextureView) mView);
          }
          removeView(mView);
        }
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
      // Beneath an outgoing view, which covers it until it shows the canvas.
      addView(mView, 0);
      // ReactViewGroup.requestLayout() is a deliberate no-op, so addView outside
      // the layout pass never lays the child out; do it by hand or the new view
      // stays 0x0 and never gets a surface.
      layoutChild();
      // Swapping a canvas on screen to a TextureView draws no empty frame: the
      // new view gets its texture, and the latest frame, before its first draw
      // rather than during it.
      if (kind.receivesSurfaceBeforeFirstDraw(isShown(), getWidth(), getHeight())) {
        ((WebGPUTextureView) mView).supplySurfaceTexture(getWidth(), getHeight());
      }
    } else if (kind == BackingViewKind.TEXTURE_VIEW) {
      ((WebGPUTextureView) mView).setOpaque(mOpaque);
    } else if (kind == BackingViewKind.SURFACE_VIEW) {
      ((WebGPUSurfaceView) mView).setOpaque(mOpaque);
    }
    // WebGPUHardwareBufferView always composites with alpha; opacity is a no-op.
  }

  private void retireSurfaceView(WebGPUSurfaceView view) {
    view.retire();
    mRetiredSurfaceView = view;
    // The window recomputes its transparent region only on a layout pass,
    // which React Native's requestLayout never reaches.
    requestTransparentRegion(view);
  }

  private void removeRetiredSurfaceView() {
    WebGPUSurfaceView retired = mRetiredSurfaceView;
    if (retired == null) {
      return;
    }
    mRetiredSurfaceView = null;
    removeView(retired);
  }

  @Override
  protected boolean drawChild(Canvas canvas, View child, long drawingTime) {
    return child != mRetiredSurfaceView && super.drawChild(canvas, child, drawingTime);
  }

  @Override
  protected void onWindowVisibilityChanged(int visibility) {
    super.onWindowVisibilityChanged(visibility);
    if (visibility != VISIBLE) {
      removeRetiredSurfaceView();
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
    WebGPUTextureView outgoing = mOutgoingView.held();
    if (outgoing != null) {
      outgoing.layout(0, 0, getMeasuredWidth(), getMeasuredHeight());
    }
  }

  // A SurfaceView asks its parent for the window's transparent region as it
  // attaches and, from Android 11, as it opens its hole
  // (SurfaceView.performDrawFinished).
  @Override
  public void requestTransparentRegion(View child) {
    super.requestTransparentRegion(child);
    if (child == mView) {
      mOutgoingView.onTransparentRegionRequested();
    }
  }

  @Override
  protected void onDetachedFromWindow() {
    mOutgoingView.release();
    removeRetiredSurfaceView();
    super.onDetachedFromWindow();
  }

  @Override
  public void surfaceCreated(Surface surface, long firstFrameToken, boolean beforeFirstDraw) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    // The outgoing view still shows the canvas while it is held: the new
    // surface starts from the frame on screen now.
    WebGPUTextureView outgoing = mOutgoingView.held();
    if (outgoing != null) {
      keepTextureViewFrame(outgoing);
    }
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
      firstFrameToken,
      beforeFirstDraw);
  }

  /**
   * The first frame was presented into the surface firstFrameToken names.
   * Called from native on the thread that presented it.
   */
  @DoNotStrip
  public void onFirstFramePresented(long firstFrameToken) {
    post(() -> {
      if (mView instanceof WebGPUSurfaceView) {
        WebGPUSurfaceView surfaceView = (WebGPUSurfaceView) mView;
        // Before the SurfaceView reports its draw: a report made during a
        // window relayout opens the hole at once, even on Android 13.
        if (surfaceView.hasSurface(firstFrameToken)) {
          mOutgoingView.onFirstFramePresented();
        }
        surfaceView.onFirstFramePresented(firstFrameToken);
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
    mOutgoingView.release();
    removeRetiredSurfaceView();
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
    long firstFrameToken,
    boolean adoptsBeforeFirstDraw
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
