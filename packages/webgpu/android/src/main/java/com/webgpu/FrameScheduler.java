package com.webgpu;

/**
 * Decides when a view presenting through BlitPresenter.h puts the canvas's latest frame on its
 * surface. A SurfaceView presents on the next vsync, since the compositor takes its buffer there.
 * A TextureView presents behind the draw its window has pending, so that the window has latched
 * the previous frame by then: its buffer queue holds a single frame and silently replaces one not
 * latched yet, which shows every other frame when presents run ahead of the window's draws.
 *
 * <p>Keeps at most one frame callback and one posted present outstanding. Main thread only. The
 * host does the actual posting, so this class stays plain Java (see FrameSchedulerTest).
 */
final class FrameScheduler {

  /** The Android view the frames are presented on. */
  enum Kind {
    SURFACE_VIEW,
    TEXTURE_VIEW
  }

  /** What the scheduler asks of the view. */
  interface Host {
    /** Calls onFrame() on the next Choreographer frame. */
    void postFrameCallback();

    /** Calls onPosted() from the main looper, behind any draw the window has pending. */
    void postBehindPendingDraw();

    /**
     * Presents the latest frame if the surface can take it. Returns whether a frame is still
     * waiting because the surface could not take it, to be retried on the next vsync.
     */
    boolean presentFrame();
  }

  private final Host mHost;
  private final Kind mKind;
  private boolean mFrameCallbackPosted;
  private boolean mPresentPosted;

  FrameScheduler(Host host, Kind kind) {
    mHost = host;
    mKind = kind;
  }

  /** A frame is ready, or the view can take one again. */
  void requestFrame() {
    // A frame callback still outstanding (a retry, see presentOrRetry) presents the new frame
    // along with the waiting one on that vsync.
    if (mFrameCallbackPosted) {
      return;
    }
    if (presentsOnVsync()) {
      postFrameCallback();
    } else {
      postBehindPendingDraw();
    }
  }

  /** The Choreographer frame asked for with Host.postFrameCallback(). */
  void onFrame() {
    mFrameCallbackPosted = false;
    if (presentsOnVsync()) {
      presentOrRetry();
    } else {
      postBehindPendingDraw();
    }
  }

  /** The message posted with Host.postBehindPendingDraw(). */
  void onPosted() {
    mPresentPosted = false;
    presentOrRetry();
  }

  /** The surface went away, and the host removed both callbacks. */
  void cancel() {
    mFrameCallbackPosted = false;
    mPresentPosted = false;
  }

  private boolean presentsOnVsync() {
    return mKind == Kind.SURFACE_VIEW;
  }

  // A frame the surface could not take is tried again on the next vsync, never straight away.
  private void presentOrRetry() {
    if (mHost.presentFrame()) {
      postFrameCallback();
    }
  }

  private void postFrameCallback() {
    if (!mFrameCallbackPosted) {
      mFrameCallbackPosted = true;
      mHost.postFrameCallback();
    }
  }

  private void postBehindPendingDraw() {
    if (!mPresentPosted) {
      mPresentPosted = true;
      mHost.postBehindPendingDraw();
    }
  }
}
