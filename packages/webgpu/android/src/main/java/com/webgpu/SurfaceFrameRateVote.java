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
 * attach(), detach() and the vote itself run on the main thread, where the
 * view's display can be read; onAnimating() runs on whichever thread presents
 * (the JS thread, the Reanimated UI runtime or a worklet runtime), so the
 * state is locked.
 */
final class SurfaceFrameRateVote {
  private static final String TAG = "SurfaceFrameRateVote";

  // The vote is taken back once the canvas has not animated for this long,
  // whether it stops drawing or drops back to a few redraws a second. Native
  // code decides what animating means (presents faster than 40 fps, see
  // AnimationReporter in cpp-adapter.cpp) and reports it at least every 100
  // ms while it lasts. The threshold is absolute rather than relative to the
  // current refresh rate, so that a canvas that only manages 60 fps at 120 Hz
  // keeps its vote rather than oscillating between the two rates. A canvas
  // that redraws a few times a second never votes.
  private static final long IDLE_AFTER_NANOS = 500_000_000L;

  private final View mView;
  private final Handler mHandler = new Handler(Looper.getMainLooper());
  private final Runnable mVote = this::vote;
  private final Runnable mIdleCheck = this::checkIdle;

  @Nullable
  private Surface mSurface;
  private boolean mVoting = false;
  private boolean mVotePending = false;
  // The last time native code reported the canvas animating; 0 until then.
  private long mLastAnimatingNanos = 0;

  /** The view the surface belongs to; its display decides the rate to ask for. */
  SurfaceFrameRateVote(View view) {
    mView = view;
  }

  /** The surface was created: it is voted on until detach(). Main thread. */
  synchronized void attach(Surface surface) {
    detach();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
      return;
    }
    mSurface = surface;
  }

  /** The canvas is animating (presenting frames back to back). Any thread. */
  synchronized void onAnimating() {
    if (mSurface == null) {
      return;
    }
    mLastAnimatingNanos = System.nanoTime();
    if (!mVoting && !mVotePending) {
      mVotePending = true;
      mHandler.post(mVote);
    }
  }

  /** The surface is going away (it is still valid here) or the view left its window. Main thread. */
  synchronized void detach() {
    if (mSurface == null) {
      return;
    }
    withdraw();
    mHandler.removeCallbacks(mVote);
    mHandler.removeCallbacks(mIdleCheck);
    mVotePending = false;
    mLastAnimatingNanos = 0;
    mSurface = null;
  }

  private synchronized void vote() {
    mVotePending = false;
    if (mSurface == null || mVoting) {
      return;
    }
    float refreshRate = resolveMaxRefreshRate();
    if (refreshRate <= 0 || !mSurface.isValid()) {
      return;
    }
    try {
      // This form only switches when the display can do so seamlessly, like
      // the vote the window casts for its own views: a canvas animation is
      // not worth a blank screen.
      mSurface.setFrameRate(refreshRate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
      mVoting = true;
      scheduleIdleCheck(IDLE_AFTER_NANOS);
    } catch (RuntimeException e) {
      Log.w(TAG, "Could not vote the refresh rate, leaving it to the display", e);
      detach();
    }
  }

  private void withdraw() {
    if (!mVoting) {
      return;
    }
    mVoting = false;
    if (mSurface == null || !mSurface.isValid()) {
      return;
    }
    try {
      mSurface.setFrameRate(0, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
    } catch (RuntimeException e) {
      Log.w(TAG, "Could not withdraw the refresh rate vote", e);
    }
  }

  private void scheduleIdleCheck(long delayNanos) {
    mHandler.postDelayed(mIdleCheck, (delayNanos + 999_999) / 1_000_000);
  }

  private synchronized void checkIdle() {
    if (!mVoting) {
      return;
    }
    long idleNanos = System.nanoTime() - mLastAnimatingNanos;
    if (idleNanos >= IDLE_AFTER_NANOS) {
      withdraw();
    } else {
      scheduleIdleCheck(IDLE_AFTER_NANOS - idleNanos);
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
