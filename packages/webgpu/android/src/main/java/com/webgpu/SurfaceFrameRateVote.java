package com.webgpu;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * Votes the display's refresh rate for a SurfaceView canvas while it animates.
 *
 * A SurfaceView is composited in a layer of its own, outside the window that
 * votes for the app's views, so SurfaceFlinger picks the layer's refresh rate
 * from its present history alone: a canvas drawing every frame on a display
 * idling at 60 Hz looks like 60 Hz content and never gets more. Since Android
 * 11 a layer can say what it wants with Surface.setFrameRate. Once the canvas
 * presents frames back to back, this asks for the highest rate the display can
 * switch to seamlessly, as the window does for its own views, and takes the
 * vote back once the animation stops, whether the canvas stops drawing or
 * drops back to a few redraws a second: a layer with a vote counts as active
 * for SurfaceFlinger even when it no longer draws.
 *
 * Native code tells onAnimating() once per animation, on the thread that
 * presents (the JS thread, the Reanimated UI runtime or a worklet runtime),
 * and the vote is cast right there, with no hop to a main thread that may be
 * busy. The rate is resolved ahead of time on the main thread, where the
 * view's display can be read. The idle check runs on the main thread and asks
 * native code how long ago the canvas last animated; once it has stopped, it
 * takes the vote back and native code calls onAnimating() again at the next
 * animation. The lock only guards the state: the binder calls
 * (Surface.setFrameRate) are made outside it.
 */
final class SurfaceFrameRateVote {
  private static final String TAG = "SurfaceFrameRateVote";

  // The vote is taken back once the canvas has not animated for this long,
  // whether it stops drawing or drops back to a few redraws a second. Native
  // code decides what animating means (presents faster than 40 fps, see
  // AnimationReporter in cpp-adapter.cpp). The threshold is absolute rather
  // than relative to the current refresh rate, so that a canvas that only
  // manages 60 fps at 120 Hz keeps its vote rather than oscillating between
  // the two rates. A canvas that redraws a few times a second never votes.
  static final long IDLE_AFTER_NANOS = 500_000_000L;

  /** Native side of the vote, see AnimationReporter in cpp-adapter.cpp. */
  interface AnimationSource {
    /**
     * How much longer the canvas must stay still before it counts as idle,
     * or 0 once it does, in which case the next animation calls
     * onAnimating() again. Main thread.
     */
    long releaseIfIdle(long idleAfterNanos);
  }

  private final View mView;
  private final AnimationSource mSource;
  private final Handler mHandler = new Handler(Looper.getMainLooper());
  private final Runnable mIdleCheck = this::checkIdle;

  @Nullable
  private Surface mSurface;
  private boolean mVoting = false;
  // The rate to ask for, resolved on the main thread; 0 when unknown, in
  // which case animations don't vote until the next idle check resolves it.
  private float mRefreshRate = 0f;

  /** The view the surface belongs to; its display decides the rate to ask for. */
  SurfaceFrameRateVote(View view, AnimationSource source) {
    mView = view;
    mSource = source;
  }

  /** The surface was created: it is voted on until detach(). Main thread. */
  void attach(Surface surface) {
    detach();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    float refreshRate = resolveMaxRefreshRate();
    synchronized (this) {
      mSurface = surface;
      mRefreshRate = refreshRate;
    }
  }

  /**
   * The canvas started animating (presenting frames back to back). Called
   * once per animation, on the presenting thread.
   */
  void onAnimating() {
    Surface surface;
    float refreshRate;
    synchronized (this) {
      if (mSurface == null) {
        return;
      }
      surface = mSurface;
      refreshRate = mRefreshRate;
      mVoting = refreshRate > 0;
    }
    // Whether it votes or not, the idle check re-arms native code once the
    // animation stops.
    mHandler.postDelayed(mIdleCheck, IDLE_AFTER_NANOS / 1_000_000);
    if (refreshRate > 0) {
      // This form only switches when the display can do so seamlessly, like
      // the vote the window casts for its own views: a canvas animation is
      // not worth a blank screen. Surface.setFrameRate is thread-safe.
      setFrameRate(surface, refreshRate);
    }
  }

  /** The surface is going away (it is still valid here) or the view left its window. Main thread. */
  void detach() {
    Surface surface;
    boolean voting;
    synchronized (this) {
      surface = mSurface;
      voting = mVoting;
      mSurface = null;
      mVoting = false;
    }
    mHandler.removeCallbacks(mIdleCheck);
    // A vote cast concurrently on the presenting thread may land after this;
    // it dies with the surface, which is going away.
    if (surface != null && voting) {
      setFrameRate(surface, 0);
    }
  }

  private void checkIdle() {
    long remainingNanos = mSource.releaseIfIdle(IDLE_AFTER_NANOS);
    if (remainingNanos > 0) {
      mHandler.postDelayed(mIdleCheck, (remainingNanos + 999_999) / 1_000_000);
      return;
    }
    // Native code is re-armed: the next onAnimating() can only come after
    // this point, so it doesn't race with the withdrawal below.
    float refreshRate = resolveMaxRefreshRate();
    Surface surface;
    boolean voting;
    synchronized (this) {
      surface = mSurface;
      voting = mVoting;
      mVoting = false;
      if (surface != null) {
        // Follows the display the view is on, for the next animation.
        mRefreshRate = refreshRate;
      }
    }
    if (surface != null && voting) {
      setFrameRate(surface, 0);
    }
  }

  private static void setFrameRate(Surface surface, float refreshRate) {
    if (!surface.isValid()) {
      return;
    }
    try {
      surface.setFrameRate(refreshRate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
    } catch (RuntimeException e) {
      Log.w(TAG, "Could not set the surface's frame rate, leaving it to the display", e);
    }
  }

  // The highest rate a seamless switch from the current mode can reach, which
  // is all the vote may ask for. Resolved when the vote is cast, so that it
  // follows the display the view is on.
  private float resolveMaxRefreshRate() {
    Display display = mView.getDisplay();
    if (display == null) {
      return 0f;
    }
    Display.Mode current = display.getMode();
    float maxRefreshRate = current.getRefreshRate();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
      for (float refreshRate : current.getAlternativeRefreshRates()) {
        maxRefreshRate = Math.max(maxRefreshRate, refreshRate);
      }
    } else {
      // Android 11 does not tell which modes are a seamless switch away: the
      // ones at the current resolution usually are.
      for (Display.Mode mode : display.getSupportedModes()) {
        if (mode.getPhysicalWidth() == current.getPhysicalWidth()
            && mode.getPhysicalHeight() == current.getPhysicalHeight()) {
          maxRefreshRate = Math.max(maxRefreshRate, mode.getRefreshRate());
        }
      }
    }
    return maxRefreshRate;
  }
}
