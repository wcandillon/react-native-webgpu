package com.webgpu;

import android.content.Context;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

/**
 * The Android view behind <Canvas>. It is backed by a TextureView or a
 * SurfaceView, named by the androidBackingView prop and created once, from the props of the first transaction, for the life of
 * the view (see updateView()).
 */
public class WebGPUView extends ReactViewGroup implements WebGPUAPI {
  private static final String TAG = "WebGPUView";

  private int mContextId;
  // Props. opaque is applied in place, and so are the z-order props where
  // Android allows it (see takesZOrder); backingView is read once, when the
  // backing view is created (see updateView()).
  private BackingViewKind mBackingView = BackingViewKind.TEXTURE_VIEW;
  private boolean mOpaque = false;
  private boolean mZOrderOnTop = false;
  private boolean mZOrderMediaOverlay = false;
  private WebGPUModule mModule;
  // The backing view, created by the first prop transaction and kept, with
  // the kind it was created with. The kind is null until then: it is what
  // tells a mounted canvas apart.
  private View mView = null;
  @Nullable
  private BackingViewKind mKind = null;
  // Set by destroy(): the canvas is gone.
  private boolean mDestroyed = false;
  // Only a SurfaceView votes: a TextureView is composited into the window,
  // which votes for it.
  private final SurfaceFrameRateVote mFrameRateVote = new SurfaceFrameRateVote(
    this, idleAfterNanos -> releaseFrameRateVoteIfIdle(mContextId, idleAfterNanos));

  WebGPUView(Context context) {
    super(context);
    // ReactViewGroup disables clipChildren (RN overflow is handled with canvas
    // clipping instead). Android copies the parent's clipChildren into each
    // child's RenderNode.clipToBounds, and HWUI treats a damaged node without
    // it as unbounded damage (RenderNode::damageSelf). A TextureView child
    // invalidates on every frame, so without this every non-opaque canvas
    // frame repaints the whole window. Our only child
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

  // The backing view is created from the props of the first transaction and
  // kept (see updateView()). React calls a setter again only when its prop
  // changed, so a call on a mounted canvas is a change: the setters below
  // apply it in place where the view can take it, else they ignore it and
  // say so (a canvas that needs another backing view is remounted, with a
  // new key). The JS side warns about the same changes in development (see
  // Canvas.tsx), mirroring these rules.

  public void setBackingView(@Nullable String value) {
    if (mKind != null) {
      warnIgnoredOnceMounted("android.backingView");
      return;
    }
    mBackingView = BackingViewKind.fromBackingView(value);
  }

  public void setOpaque(boolean value) {
    mOpaque = value;
  }

  public void setZOrderOnTop(boolean value) {
    if (takesZOrder("android.zOrderOnTop")) {
      mZOrderOnTop = value;
      applyZOrder();
    }
  }

  public void setZOrderMediaOverlay(boolean value) {
    if (takesZOrder("android.zOrderMediaOverlay")) {
      mZOrderMediaOverlay = value;
      applyZOrder();
    }
  }

  // Whether a z-order prop is taken. Before the backing view exists it is
  // read when the view is created. After that only a SurfaceView has a
  // z-order of its own (elsewhere the prop never applied, and the change is
  // moot), and SurfaceView moves a live surface since Android 11, for an app
  // targeting it; before that, the surface reads it when it is created.
  private boolean takesZOrder(String prop) {
    if (mKind == null) {
      return true;
    }
    if (mKind != BackingViewKind.SURFACE_VIEW) {
      return false;
    }
    boolean inPlace = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
        && getContext().getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.R;
    if (!inPlace) {
      warnIgnoredOnceMounted(prop);
    }
    return inPlace;
  }

  private void applyZOrder() {
    if (mKind == BackingViewKind.SURFACE_VIEW) {
      ((WebGPUSurfaceView) mView).setZOrder(mZOrderOnTop, mZOrderMediaOverlay);
    }
  }

  private void warnIgnoredOnceMounted(String prop) {
    Log.w(TAG, "Ignoring a change of " + prop + " on a mounted canvas: the backing view"
        + " is chosen when the canvas mounts. Remount the canvas (give it a new key)"
        + " to change it.");
  }

  // Apply the complete prop transaction once, after contextId and all rendering
  // options have arrived. The first one creates the backing view, which is
  // then kept: a SurfaceView must know its z-order before it attaches (before
  // Android 11), and keeping a canvas on screen while its backing view is
  // replaced takes hold-and-retire logic specific to each Android release (see
  // PR #563, and react-native-skia's #4189 and #4192), which this view does
  // not carry. A canvas that needs another backing view is remounted instead,
  // and is off screen for a frame or two until the new view draws its first
  // frame. Later transactions apply the opacity in place (the setters decide
  // what else is taken).
  public void updateView() {
    if (mKind != null) {
      if (mKind == BackingViewKind.TEXTURE_VIEW) {
        ((WebGPUTextureView) mView).setOpaque(mOpaque);
      } else {
        ((WebGPUSurfaceView) mView).setOpaque(mOpaque);
      }
      return;
    }
    mKind = mBackingView;
    Context ctx = getContext();
    if (mKind == BackingViewKind.TEXTURE_VIEW) {
      mView = new WebGPUTextureView(ctx, this, mOpaque);
    } else {
      mView = new WebGPUSurfaceView(ctx, this, mZOrderOnTop, mZOrderMediaOverlay, mOpaque);
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
    boolean votesFrameRate = mKind == BackingViewKind.SURFACE_VIEW;
    if (votesFrameRate) {
      mFrameRateVote.attach(surface);
    }
    boolean latchOnMainThread = mKind == BackingViewKind.TEXTURE_VIEW;
    onSurfaceCreate(surface, mContextId, width, height, votesFrameRate, latchOnMainThread);
  }

  @Override
  public void surfaceChanged(Surface surface) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    onSurfaceChanged(surface, mContextId, width, height);
  }

  @Override
  public void surfaceParked() {
    parkSurface(mContextId);
  }

  @Override
  public boolean surfaceUnparked() {
    return unparkSurface(mContextId);
  }

  @Override
  public void backingViewLost(View view) {
    // Posted by the view; it may have been replaced or this canvas dropped since.
    // A view that left its window again in the meantime has parked the canvas
    // once more: it retries when it is attached again (another unpark that
    // fails), and replacing it now would leave the new view stuck behind that
    // park, since removing a detached view reports no detach.
    if (view != mView || mDestroyed || mKind != BackingViewKind.TEXTURE_VIEW
        || !view.isAttachedToWindow()) {
      return;
    }
    WebGPUTextureView lost = (WebGPUTextureView) mView;
    // Released first, so its SurfaceTexture goes with it rather than being kept.
    lost.release();
    removeView(lost);
    mView = new WebGPUTextureView(getContext(), this, mOpaque);
    addView(mView);
    layoutChild();
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

  /**
   * Called from native (cpp-adapter.cpp) right after a frame is presented to
   * a TextureView from the main thread, so that the draw of this frame shows
   * it (see WebGPUTextureView.latchPresentedFrame).
   */
  @DoNotStrip
  void onFramePresentedOnMainThread() {
    if (mView instanceof WebGPUTextureView) {
      ((WebGPUTextureView) mView).latchPresentedFrame();
    }
  }

  /**
   * Called from WebGPUViewManager.onDropViewInstance when React removes this
   * view: the view dies with its Canvas, so it retires the registry entry.
   */
  public void destroy() {
    mDestroyed = true;
    mFrameRateVote.detach();
    onViewDestroyed(mContextId);
    if (mView instanceof WebGPUTextureView) {
      ((WebGPUTextureView) mView).release();
    }
  }

  // reportAnimation asks native to call onAnimating() when frames start being
  // presented to this surface back to back; latchOnMainThread, to call
  // onFramePresentedOnMainThread() after each frame presented from the main
  // thread.
  @DoNotStrip
  private native void onSurfaceCreate(
    Surface surface,
    int contextId,
    float width,
    float height,
    boolean reportAnimation,
    boolean latchOnMainThread
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
  private native void parkSurface(int contextId);

  @DoNotStrip
  private native boolean unparkSurface(int contextId);

  @DoNotStrip
  private native void onViewDestroyed(int contextId);

  // See SurfaceFrameRateVote.AnimationSource.
  @DoNotStrip
  private native long releaseFrameRateVoteIfIdle(int contextId, long idleAfterNanos);

}
