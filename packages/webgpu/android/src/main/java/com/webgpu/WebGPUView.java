package com.webgpu;

import android.content.Context;
import android.view.Surface;
import android.view.View;

import java.util.concurrent.atomic.AtomicLong;

import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.react.views.view.ReactViewGroup;

public class WebGPUView extends ReactViewGroup implements WebGPUAPI {
  private static final AtomicLong NEXT_SURFACE_OWNER_ID = new AtomicLong(1);
  private static final int SURFACE_PUBLISH_FAILED = 0;
  private static final int SURFACE_PUBLISH_REGISTERED_OFFSCREEN = 1;
  private static final int SURFACE_PUBLISH_ONSCREEN = 2;

  private final long mSurfaceOwnerId = createSurfaceOwnerId();
  private long mSessionId;
  private int mContextId;
  private boolean mTransparent = false;
  private boolean mDisposed = false;
  private boolean mSurfaceRegistered = false;
  private boolean mSurfaceOnscreen = false;
  private Surface mCurrentSurface;
  private View mView;

  WebGPUView(Context context) {
    super(context);
  }

  public void setSessionId(long sessionId) {
    if (mSessionId == sessionId) {
      return;
    }
    destroyRegisteredSurface();
    mSessionId = sessionId;
  }

  public void setContextId(int contextId) {
    if (mContextId == contextId) {
      return;
    }
    destroyRegisteredSurface();
    mContextId = contextId;
  }

  public void commitProperties() {
    publishCurrentSurface();
  }

  public void setTransparent(boolean value) {
    if (mDisposed || (value == mTransparent && mView != null)) {
      return;
    }

    View previousView = mView;
    if (previousView != null) {
      // Preserve the GPUCanvasContext while swapping SurfaceView and
      // TextureView. The replacement surface claims the same native owner and
      // copies the offscreen frame back to the new onscreen surface.
      moveRegisteredSurfaceOffscreen();
      mCurrentSurface = null;
      mView = null;
      removeView(previousView);
    }

    mTransparent = value;
    mView = mTransparent
      ? new WebGPUTextureView(getContext(), this)
      : new WebGPUSurfaceView(getContext(), this);
    addView(mView);
  }

  public void dispose() {
    if (mDisposed) {
      return;
    }

    mDisposed = true;
    destroyRegisteredSurface();
    mCurrentSurface = null;

    View previousView = mView;
    mView = null;
    if (previousView != null) {
      removeView(previousView);
    }
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    if (mView != null) {
      mView.layout(0, 0, getMeasuredWidth(), getMeasuredHeight());
    }
  }

  @Override
  public void surfaceCreated(View source, Surface surface) {
    if (!acceptsCallbackFrom(source)) {
      return;
    }
    if (mCurrentSurface != surface) {
      moveRegisteredSurfaceOffscreen();
    }
    mCurrentSurface = surface;
    publishCurrentSurface();
  }

  @Override
  public void surfaceChanged(View source, Surface surface) {
    if (!acceptsCallbackFrom(source)) {
      return;
    }
    boolean surfaceWasReplaced = mCurrentSurface != surface;
    if (surfaceWasReplaced) {
      moveRegisteredSurfaceOffscreen();
    }
    mCurrentSurface = surface;
    if (!hasValidIdentity()) {
      return;
    }

    if (!mSurfaceRegistered || !mSurfaceOnscreen || surfaceWasReplaced) {
      publishCurrentSurface();
      return;
    }

    float density = getResources().getDisplayMetrics().density;
    onSurfaceChanged(
      mSessionId,
      mSurfaceOwnerId,
      surface,
      mContextId,
      getWidth() / density,
      getHeight() / density
    );
  }

  @Override
  public void surfaceDestroyed(View source) {
    if (!acceptsCallbackFrom(source)) {
      return;
    }
    mCurrentSurface = null;
    destroyRegisteredSurface();
  }

  @Override
  public void surfaceOffscreen(View source) {
    if (!acceptsCallbackFrom(source)) {
      return;
    }
    mCurrentSurface = null;
    if (mSurfaceRegistered && hasValidIdentity()) {
      switchToOffscreenSurface(mSessionId, mSurfaceOwnerId, mContextId);
      mSurfaceOnscreen = false;
    }
  }

  private boolean acceptsCallbackFrom(View source) {
    return !mDisposed && source == mView;
  }

  private boolean hasValidIdentity() {
    return mSessionId > 0 && mContextId > 0;
  }

  private void publishCurrentSurface() {
    if (mDisposed
      || mCurrentSurface == null
      || !hasValidIdentity()
      || (mSurfaceRegistered && mSurfaceOnscreen)) {
      return;
    }

    float density = getResources().getDisplayMetrics().density;
    boolean wasRegistered = mSurfaceRegistered;
    int publishStatus = onSurfaceCreate(
      mSessionId,
      mSurfaceOwnerId,
      mCurrentSurface,
      mContextId,
      getWidth() / density,
      getHeight() / density
    );
    mSurfaceRegistered = wasRegistered
      || publishStatus == SURFACE_PUBLISH_REGISTERED_OFFSCREEN
      || publishStatus == SURFACE_PUBLISH_ONSCREEN;
    mSurfaceOnscreen = publishStatus == SURFACE_PUBLISH_ONSCREEN;
  }

  private void destroyRegisteredSurface() {
    if (mSurfaceRegistered && hasValidIdentity()) {
      onSurfaceDestroy(mSessionId, mSurfaceOwnerId, mContextId);
    }
    mSurfaceRegistered = false;
    mSurfaceOnscreen = false;
  }

  private void moveRegisteredSurfaceOffscreen() {
    if (mSurfaceRegistered && mSurfaceOnscreen && hasValidIdentity()) {
      switchToOffscreenSurface(mSessionId, mSurfaceOwnerId, mContextId);
    }
    mSurfaceOnscreen = false;
  }

  private static long createSurfaceOwnerId() {
    long ownerId = NEXT_SURFACE_OWNER_ID.getAndIncrement();
    if (ownerId <= 0) {
      throw new IllegalStateException("WebGPU surface owner ID overflow");
    }
    return ownerId;
  }

  @DoNotStrip
  private native int onSurfaceCreate(
    long sessionId,
    long surfaceOwnerId,
    Surface surface,
    int contextId,
    float width,
    float height
  );

  @DoNotStrip
  private native void onSurfaceChanged(
    long sessionId,
    long surfaceOwnerId,
    Surface surface,
    int contextId,
    float width,
    float height
  );

  @DoNotStrip
  private native void onSurfaceDestroy(long sessionId, long surfaceOwnerId, int contextId);

  @DoNotStrip
  private native void switchToOffscreenSurface(
    long sessionId,
    long surfaceOwnerId,
    int contextId
  );
}
