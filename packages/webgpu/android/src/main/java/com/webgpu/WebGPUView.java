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
  // Props. opaque is applied in place, and so is zOrderOnTop where Android
  // allows it (see setZOrderOnTop); surfaceType is read once, when the
  // backing view is created (see updateView()).
  private boolean mOpaque = true;
  private String mSurfaceType = "auto";
  private boolean mZOrderOnTop = false;
  private WebGPUModule mModule;
  // The backing view, created by the first prop transaction and kept, with
  // the kind it was created with. The kind is -1 until then: it is what
  // tells a mounted canvas apart.
  private View mView = null;
  private int mKind = -1;
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

  // The backing view is chosen from the props of the first transaction and
  // kept (see updateView()). React calls a setter again only when its prop
  // changed, so a call on a mounted canvas is a change: the setters below
  // apply it in place where the view can take it, else they ignore it and
  // say so (a canvas that needs another backing view is remounted, with a
  // new key). The JS side warns about the same changes in development (see
  // Canvas.tsx), mirroring these rules.

  public void setSurfaceType(String value) {
    if (mKind != -1) {
      warnIgnoredOnceMounted("android.surfaceType");
      return;
    }
    mSurfaceType = value;
  }

  public void setZOrderOnTop(boolean value) {
    if (mKind == -1) {
      mZOrderOnTop = value;
      return;
    }
    // Only a SurfaceView has a z-order of its own: elsewhere the prop never
    // applied, and the change is moot.
    if (mKind != KIND_SURFACE_VIEW) {
      return;
    }
    if (!zOrderChangesInPlace()) {
      warnIgnoredOnceMounted("android.zOrderOnTop");
      return;
    }
    mZOrderOnTop = value;
    ((WebGPUSurfaceView) mView).setZOrderOnTop(value);
  }

  private void warnIgnoredOnceMounted(String prop) {
    Log.w(TAG, "Ignoring a change of " + prop + " on a mounted canvas: the backing view"
        + " is chosen when the canvas mounts. Remount the canvas (give it a new key)"
        + " to change it.");
  }

  // SurfaceView.setZOrderOnTop() moves a live surface since Android 11, for
  // an app targeting it; before that, the surface reads it when it is
  // created (see updateView()).
  private boolean zOrderChangesInPlace() {
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
        && getContext().getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.R;
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
  // then kept: a SurfaceView must know its z-order before it attaches (before
  // Android 11), and replacing a backing view on screen hands the canvas from
  // one view to another, which no release of Android does without a frame of
  // nothing, or of black, in between (the two views are composited by
  // different frames). A canvas that needs another backing view is remounted
  // instead. Later transactions apply the opacity in place (the setters decide
  // what else is taken); WebGPUHardwareBufferView always composites with
  // alpha, so there it is a no-op.
  public void updateView() {
    if (mKind != -1) {
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
