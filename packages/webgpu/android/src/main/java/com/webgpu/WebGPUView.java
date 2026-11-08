package com.webgpu;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.util.Log;
import android.view.Choreographer;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

/**
 * The Android view behind <Canvas>. It is backed by a TextureView, a
 * SurfaceView or a HardwareBufferView, named by the androidBackingView prop
 * and created once, from the props of the first transaction, for the life of
 * the view (see updateView()), that the canvas's frames are presented into:
 * by the app's render loop itself, into the window's own buffers, for a canvas
 * presenting direct (the default), or by the present thread, when the backing
 * view can show them, for a composited one (the presentation prop, see
 * SurfaceInfo::Presentation and rnsurface::FrameScheduler on the native
 * side).
 *
 * Threading: the view and its surface callbacks are on the main thread. The
 * window belongs to the present thread (see PresentThread), where the frames
 * are presented, the frame scheduler (rnsurface::FrameScheduler, on the
 * native side) and the Choreographer callback live: the surface callbacks
 * post the attach, the resize and the detach there, so that the main thread
 * never waits for a present in flight. A present holds the window for as long
 * as the window takes to hand a buffer out and take it back, which waits for
 * the GPU, and on a TextureView of Android 11 for the window's own draw,
 * seconds at a time; a main thread made to wait for that is an ANR. The
 * scheduler asks this view for the Choreographer's vsync (postFrameCallback)
 * and gets it back (doFrame). A TextureView tells the main thread when it drew
 * a frame (onSurfaceTextureUpdated), which hands it over to the scheduler,
 * and leaves its SurfaceTexture to the present thread, which releases it once
 * the window let go of it (see onSurfaceTextureDestroyed). A SurfaceView's
 * surface is the platform's, which destroys it as soon as surfaceDestroyed
 * returns, so there the main thread waits for the detach, for the length of
 * a present at most (see onSurfaceDestroyed). The same as React Native
 * Skia's SkiaView, whose present thread this mirrors.
 */
public class WebGPUView extends ReactViewGroup implements WebGPUAPI, Choreographer.FrameCallback {
  private static final String TAG = "WebGPUView";

  // How long the main thread waits for the present thread to let go of a
  // SurfaceView's surface (see onSurfaceDestroyed): longer than a present
  // (a few milliseconds, behind the presents of the other canvases), shorter
  // than a stall of the window, well under the 5 seconds at which Android
  // reports an ANR, and paid once per hold-up, however many canvases go (see
  // PresentThread.isHeldUp).
  private static final long SURFACE_DETACH_TIMEOUT_MS = 200;

  private int mContextId;
  // Props. opaque is applied in place, and so are the z-order props where
  // Android allows it (see takesZOrder); backingView is read once, when the
  // backing view is created (see updateView()), and so is presentation, as the
  // view claims its canvas (see viewAttached).
  private BackingViewKind mBackingView = BackingViewKind.TEXTURE_VIEW;
  private boolean mComposited = false;
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
  // Only a SurfaceView votes: a TextureView or HardwareBufferView is
  // composited into the window, which votes for it.
  private final SurfaceFrameRateVote mFrameRateVote = new SurfaceFrameRateVote(
    this, idleAfterNanos -> releaseFrameRateVoteIfIdle(mContextId, idleAfterNanos));

  // Set by destroy(), on the main thread: no native method is called from
  // there after that. The window itself goes on the present thread.
  private boolean mDropped = false;

  // Whether a SurfaceView's surface is attached, for the main thread: set as
  // the attach is posted (see onSurfaceCreated), cleared as the detach is
  // (see onSurfaceDestroyed, which the backing view calls twice per surface).
  private boolean mSurfaceAttached = false;

  // The backing view's surface, by number: one more each time a surface
  // goes (see onSurfaceDestroyed, onSurfaceTextureDestroyed). A posted
  // attach or resize carries the number of the surface it is for and is
  // skipped on the present thread when that surface went meanwhile (see
  // postForSurface): an attach run after the platform destroyed the surface
  // would put a window over a surface that is gone, and ask for a frame
  // onto it. Written on the main thread, read on the present thread.
  private volatile int mSurfaceGeneration = 0;

  private final Runnable mOnFrameConsumed = this::onFrameConsumed;
  // A TextureView records its draw on the main thread before the render
  // thread takes the frame, at the sync that ends the traversal: the
  // signal is posted behind the traversal, so that the frame presented on
  // it is taken by the draw after, never by this one.
  private final Runnable mFrameConsumedPosted =
      () -> PresentThread.handler().post(mOnFrameConsumed);
  private final Runnable mCancelFrames = this::cancelFrames;

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
    boolean hardwareBufferSupported = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    mBackingView = BackingViewKind.fromBackingView(value, hardwareBufferSupported);
  }

  public void setOpaque(boolean value) {
    mOpaque = value;
  }

  // How the canvas presents (see SurfaceInfo::Presentation on the native
  // side): "composited", or "direct" (the default). Read as the view claims
  // its canvas, which a change after that does not reach; the JS side warns
  // about one.
  public void setPresentation(@Nullable String value) {
    mComposited = "composited".equals(value);
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
  // what else is taken); WebGPUHardwareBufferView always composites with
  // alpha, so there it is a no-op.
  public void updateView() {
    if (mKind != null) {
      boolean opaque = BackingViewKind.resolveOpaque(mKind, mOpaque);
      if (mKind == BackingViewKind.TEXTURE_VIEW) {
        ((WebGPUTextureView) mView).setOpaque(opaque);
      } else if (mKind == BackingViewKind.SURFACE_VIEW) {
        ((WebGPUSurfaceView) mView).setOpaque(opaque);
      }
      return;
    }
    BackingViewKind kind = mBackingView;
    mKind = kind;
    boolean opaque = BackingViewKind.resolveOpaque(kind, mOpaque);
    Context ctx = getContext();
    switch (kind) {
      case HARDWARE_BUFFER_VIEW:
        mView = new WebGPUHardwareBufferView(ctx, this);
        break;
      case TEXTURE_VIEW:
        mView = new WebGPUTextureView(ctx, this, opaque);
        break;
      default:
        mView = new WebGPUSurfaceView(ctx, this, mZOrderOnTop, mZOrderMediaOverlay, opaque);
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
    // The canvas's client size (clientWidth/clientHeight in JS) follows the
    // layout; the drawing buffer is the app's (canvas.width/height).
    if (hasNativeView()) {
      setLayoutSize(mContextId, dp(right - left), dp(bottom - top));
    }
    if (mView != null) {
      layoutChild();
    }
  }

  private float dp(int pixels) {
    return pixels / getResources().getDisplayMetrics().density;
  }

  // Frames --------------------------------------------------------------

  /**
   * The scheduler asks for the next vsync: doFrame() on this thread's
   * Choreographer. Present thread; called from native.
   */
  @DoNotStrip
  public void postFrameCallback() {
    Choreographer.getInstance().postFrameCallback(this);
  }

  // The vsync asked for with postFrameCallback(). A canvas dropped meanwhile
  // (see destroy) is no longer in the registry: native does nothing then.
  @Override
  public void doFrame(long frameTimeNanos) {
    frameCallback(mContextId);
  }

  /**
   * The TextureView drew the frame it was given: the next one can go to it
   * (see rnsurface::FrameScheduler::onFrameConsumed). Main thread, from the
   * backing view.
   */
  @Override
  public void onSurfaceTextureUpdated() {
    post(mFrameConsumedPosted);
  }

  private void onFrameConsumed() {
    frameConsumed(mContextId);
  }

  // Drops the frame callback, the frame the window took and the scheduler's
  // requests, on the present thread, where they live.
  private void cancelFrames() {
    Choreographer.getInstance().removeFrameCallback(this);
    PresentThread.handler().removeCallbacks(mOnFrameConsumed);
    cancelScheduledFrames(mContextId);
  }

  @Override
  protected void onDetachedFromWindow() {
    super.onDetachedFromWindow();
    removeCallbacks(mFrameConsumedPosted);
    PresentThread.handler().post(mCancelFrames);
  }

  /**
   * Called from native (AnimationReporter in cpp-adapter.cpp) on the present
   * thread, once per animation, only for a SurfaceView.
   */
  @DoNotStrip
  void onAnimating() {
    mFrameRateVote.onAnimating();
  }

  // Surfaces ------------------------------------------------------------

  // The callbacks come on the main thread, and post to the present thread,
  // which owns the window (see the class comment): the attach, the resize
  // and the detach run there in the order they came, behind the present in
  // flight, if any.

  // Whether the canvas is still React's: it is dropped on the main thread
  // (see destroy), and no native method is called from there after that. The
  // present thread, where the window goes, asks the registry instead.
  private boolean hasNativeView() {
    return !mDropped;
  }

  // Runs the task on the present thread, if the backing view's surface is
  // still the one the task is for (see mSurfaceGeneration). Main thread; the
  // task calls native methods.
  private void postForSurface(Runnable task) {
    int generation = mSurfaceGeneration;
    PresentThread.handler().post(() -> {
      if (generation == mSurfaceGeneration) {
        task.run();
      }
    });
  }

  // SurfaceView callbacks: the native side receives an android.view.Surface.
  // Only a SurfaceView votes (attached here): a TextureView is composited
  // into its window, which votes for it.

  @Override
  public void onSurfaceCreated(Surface surface, int width, int height) {
    mFrameRateVote.attach(surface);
    if (!hasNativeView()) {
      return;
    }
    mSurfaceAttached = true;
    float dpWidth = dp(width);
    float dpHeight = dp(height);
    viewAttached(mContextId, dpWidth, dpHeight, mComposited);
    postForSurface(() -> surfaceAvailable(mContextId, surface, true, true, dpWidth, dpHeight));
  }

  @Override
  public void onSurfaceChanged(Surface surface, int width, int height) {
    if (!hasNativeView()) {
      return;
    }
    float dpWidth = dp(width);
    float dpHeight = dp(height);
    postForSurface(() -> surfaceSizeChanged(mContextId, surface, true, true, dpWidth, dpHeight));
  }

  // Main thread, before the platform destroys a SurfaceView's surface, which
  // it does as soon as this returns: the window is detached on the present
  // thread, and the main thread waits for that, for the length of a present
  // at most. Past that, the present in flight is waiting on the window (a
  // TextureView of Android 11 holds a buffer for seconds, see
  // react-native-skia#4394 for the compile that holds Skia's): the main
  // thread gives the surface up rather than hang, and the present thread
  // skips the present onto it when it gets there, as a frame presented onto
  // a window that is gone loses the GPU device. The surface is given up before
  // the detach is posted, not once the wait has timed out (see
  // SurfaceInfo::abandon): the present thread checks and then presents, and a
  // flag set after its check would miss the present. A frame skipped onto a
  // surface that is about to be destroyed is nothing lost, and the detach
  // takes the flag down again. The wait is paid once per hold-up, not once
  // per canvas: a wait that timed out marks the present thread held up until
  // the detach it posted runs, and the canvases that go meanwhile give their
  // surface up without waiting (see PresentThread.isHeldUp).
  @Override
  public void onSurfaceDestroyed() {
    // The surface is still valid here, so the vote can be taken back.
    mFrameRateVote.detach();
    // SurfaceView reports surfaceDestroyed from its own onDetachedFromWindow,
    // and WebGPUSurfaceView calls this once more after that, a safety net for
    // the paths where it does not: nothing is attached by then.
    if (!mSurfaceAttached) {
      return;
    }
    mSurfaceAttached = false;
    // An attach or a resize still posted for this surface is stale now.
    mSurfaceGeneration++;
    surfaceAbandoned(mContextId);
    CountDownLatch detached = new CountDownLatch(1);
    // Which came first, the task or the end of the wait below: the loser
    // takes the held-up mark down, so that it never outlives the task.
    AtomicBoolean settled = new AtomicBoolean(false);
    PresentThread.handler().post(() -> {
      // The thread got here after the wait for it timed out: whatever
      // held it up is over.
      if (!settled.compareAndSet(false, true)) {
        PresentThread.setHeldUp(false);
      }
      // Whatever the window had not drawn goes with it.
      cancelFrames();
      surfaceDestroyed(mContextId);
      detached.countDown();
    });
    if (PresentThread.isHeldUp()) {
      return;
    }
    try {
      if (!detached.await(SURFACE_DETACH_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
        PresentThread.setHeldUp(true);
        // The task got there between the timeout and this: not held up.
        if (!settled.compareAndSet(false, true)) {
          PresentThread.setHeldUp(false);
        }
      }
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }
  }

  // TextureView callbacks: the native side receives the SurfaceTexture and
  // creates the Surface it draws into.

  @Override
  public void onSurfaceTextureCreated(SurfaceTexture surface, int width, int height) {
    if (!hasNativeView()) {
      return;
    }
    float dpWidth = dp(width);
    float dpHeight = dp(height);
    viewAttached(mContextId, dpWidth, dpHeight, mComposited);
    postForSurface(() -> surfaceAvailable(mContextId, surface, false, false, dpWidth, dpHeight));
  }

  @Override
  public void onSurfaceTextureChanged(SurfaceTexture surface, int width, int height) {
    if (!hasNativeView()) {
      return;
    }
    float dpWidth = dp(width);
    float dpHeight = dp(height);
    postForSurface(() -> surfaceSizeChanged(mContextId, surface, false, false, dpWidth, dpHeight));
  }

  // Main thread, as the TextureView lets go of its SurfaceTexture. The window
  // over it is detached on the present thread, and the SurfaceTexture
  // released there once it is: returning false leaves the release to this
  // view (see TextureView.SurfaceTextureListener), so the main thread waits
  // for nothing, and the SurfaceTexture outlives the present in flight.
  @Override
  public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
    // An attach or a resize still posted for this SurfaceTexture is stale:
    // the window over it goes below, and the SurfaceTexture with it.
    mSurfaceGeneration++;
    PresentThread.handler().post(() -> {
      // Whatever the window had not drawn goes with it.
      cancelFrames();
      surfaceDestroyed(mContextId);
      surfaceTexture.release();
    });
    return false;
  }

  @Override
  public int getContextId() {
    return mContextId;
  }

  /**
   * Called from WebGPUViewManager.onDropViewInstance when React removes this
   * view: the view dies with its Canvas, so it retires the registry entry. The
   * window goes on the present thread, after the present in flight, if any:
   * nothing waits for it here.
   */
  public void destroy() {
    if (mDropped) {
      return;
    }
    mDropped = true;
    mFrameRateVote.detach();
    onViewDestroyed(mContextId);
  }

  // Native (cpp-adapter.cpp) ----------------------------------------------------

  // The view claims the canvas's registry entry, on the main thread, as it
  // posts its first window (see SurfaceInfo::setViewAttached), and says how
  // the canvas presents (see SurfaceInfo::setPresentation).
  private native void viewAttached(int contextId, float dpWidth, float dpHeight,
      boolean composited);

  // isSurface tells the native side whether `surface` is an
  // android.view.Surface (SurfaceView) or a SurfaceTexture (TextureView);
  // reportAnimation asks it to call onAnimating() when frames start being
  // presented to this surface back to back (a SurfaceView's frame rate vote).
  private native void surfaceAvailable(int contextId, Object surface, boolean isSurface,
      boolean reportAnimation, float dpWidth, float dpHeight);

  private native void surfaceSizeChanged(int contextId, Object surface, boolean isSurface,
      boolean reportAnimation, float dpWidth, float dpHeight);

  private native void surfaceDestroyed(int contextId);

  private native void surfaceAbandoned(int contextId);

  private native void setLayoutSize(int contextId, float dpWidth, float dpHeight);

  // The scheduler's vsync (see postFrameCallback). Present thread.
  private native void frameCallback(int contextId);

  // The TextureView drew the frame it was given. Present thread.
  private native void frameConsumed(int contextId);

  // The scheduler forgets what it asked for (see cancelFrames). Present thread.
  private native void cancelScheduledFrames(int contextId);

  private native void onViewDestroyed(int contextId);

  // See SurfaceFrameRateVote.AnimationSource.
  @DoNotStrip
  private native long releaseFrameRateVoteIfIdle(int contextId, long idleAfterNanos);
}
