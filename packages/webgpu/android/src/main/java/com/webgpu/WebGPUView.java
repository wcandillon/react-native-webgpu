package com.webgpu;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.views.view.ReactViewGroup;

/**
 * The Android view behind <Canvas>. It is backed by a TextureView, a
 * SurfaceView or a HardwareBufferView, named by the androidBackingView prop
 * and created once, from the props of the first transaction, for the life of
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
    if (mView != null) {
      layoutChild();
    }
  }

  @Override
  public void surfaceCreated(Surface surface, long firstFrameToken) {
    float density = getResources().getDisplayMetrics().density;
    float width = getWidth() / density;
    float height = getHeight() / density;
    boolean votesFrameRate = mKind == BackingViewKind.SURFACE_VIEW;
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
