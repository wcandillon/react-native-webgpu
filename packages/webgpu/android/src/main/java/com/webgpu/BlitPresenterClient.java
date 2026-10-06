package com.webgpu;

import android.os.Handler;
import android.os.Looper;
import android.view.Choreographer;
import android.view.Surface;

import androidx.annotation.Keep;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * The view side of BlitPresenter.h, shared by {@link WebGPUBlitSurfaceView} and
 * {@link WebGPUBlitTextureView}.
 *
 * <p>WebGPU renders into textures the presenter owns. When a frame is finished the rendering thread
 * calls {@link #onNativeFrameReady()}; on the UI thread this client then copies the latest frame
 * onto the view's surface and presents it, when the {@link FrameScheduler} says so. The surface is
 * only ever used from the UI thread. Native keeps only the latest frame: one that finishes while
 * another waits replaces it.
 */
final class BlitPresenterClient implements FrameScheduler.Host, Choreographer.FrameCallback {

  // Mirrors rnwgpu::BlitPresenter::PresentResult (cpp/rnwgpu/BlitPresenter.h).
  private static final int PRESENT_IDLE = 0;
  private static final int PRESENT_PRESENTED = 1;
  private static final int PRESENT_RETRY = 2;

  private final WebGPUAPI mApi;
  private final FrameScheduler.Kind mKind;
  private final FrameScheduler mScheduler;
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());

  // Rendering thread -> UI thread wake-up, coalesced into at most one pending message.
  private final AtomicBoolean mWakePosted = new AtomicBoolean(false);
  private final Runnable mWake = this::onWake;
  private final Runnable mPosted;

  private boolean mAttached;
  // TextureView only: a buffer was queued and the window has not latched it yet.
  private boolean mAwaitingLatch;

  BlitPresenterClient(WebGPUAPI api, FrameScheduler.Kind kind) {
    mApi = api;
    mKind = kind;
    mScheduler = new FrameScheduler(this, kind);
    mPosted = mScheduler::onPosted;
  }

  private int contextId() {
    return mApi.getContextId();
  }

  // --- Surface lifecycle (UI thread) ----------------------------------------

  /** The view has a surface. dpW/dpH is the canvas-client size in dp. */
  void surfaceAvailable(Surface surface, int dpW, int dpH) {
    mAttached = true;
    mAwaitingLatch = false;
    nAttach(surface, contextId(), dpW, dpH);
    // Show the latest frame right away, if the canvas already rendered one.
    mScheduler.requestFrame();
  }

  /** The view was resized; native sizes the frames from the canvas drawing buffer itself. */
  void surfaceSizeChanged(int dpW, int dpH) {
    nSetClientSize(contextId(), dpW, dpH);
  }

  /** The surface is going away. Idempotent. Detaches synchronously, before the caller releases it. */
  void surfaceDestroyed() {
    if (!mAttached) {
      return;
    }
    mAttached = false;
    mAwaitingLatch = false;
    mMainHandler.removeCallbacks(mWake);
    mWakePosted.set(false);
    mMainHandler.removeCallbacks(mPosted);
    Choreographer.getInstance().removeFrameCallback(this);
    mScheduler.cancel();
    nDetach(contextId());
  }

  /**
   * TextureView only: the window drew the frame presented last
   * ({@link android.view.TextureView.SurfaceTextureListener#onSurfaceTextureUpdated}), so its
   * buffer is back in the queue and the next one can be presented.
   */
  void frameLatched() {
    mAwaitingLatch = false;
    mScheduler.requestFrame();
  }

  // --- Wake-up (rendering thread) --------------------------------------------

  /** Called from the rendering thread (see cpp-adapter.cpp) when a frame is finished. */
  @Keep
  private void onNativeFrameReady() {
    if (mWakePosted.compareAndSet(false, true)) {
      mMainHandler.post(mWake);
    }
  }

  private void onWake() {
    mWakePosted.set(false);
    if (mAttached) {
      mScheduler.requestFrame();
    }
  }

  // --- FrameScheduler.Host (UI thread) ---------------------------------------

  @Override
  public void postFrameCallback() {
    Choreographer.getInstance().postFrameCallback(this);
  }

  @Override
  public void postBehindPendingDraw() {
    mMainHandler.post(mPosted);
  }

  @Override
  public boolean presentFrame() {
    if (!mAttached || mAwaitingLatch) {
      // Nothing to do now; frameLatched() asks again once the window took the previous buffer.
      return false;
    }
    int result = nPresentFrame(contextId());
    if (result == PRESENT_PRESENTED && mKind == FrameScheduler.Kind.TEXTURE_VIEW) {
      mAwaitingLatch = true;
    }
    return result == PRESENT_RETRY;
  }

  // --- Choreographer.FrameCallback -------------------------------------------

  @Override
  public void doFrame(long frameTimeNanos) {
    mScheduler.onFrame();
  }

  // --- Native (cpp-adapter.cpp) ----------------------------------------------

  private native void nAttach(Surface surface, int contextId, int dpW, int dpH);

  private native void nSetClientSize(int contextId, int dpW, int dpH);

  /** Returns one of the PRESENT_* values. */
  private native int nPresentFrame(int contextId);

  private native void nDetach(int contextId);
}
