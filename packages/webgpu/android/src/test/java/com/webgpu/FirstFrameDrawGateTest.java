package com.webgpu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;

public class FirstFrameDrawGateTest {
  private static final long FIRST_SURFACE = 1;
  private static final long SECOND_SURFACE = 2;

  private static final class RecordingHost implements FirstFrameDrawGate.Host {
    final List<Runnable> posted = new ArrayList<>();
    final List<Long> delays = new ArrayList<>();
    int invalidations = 0;

    @Override
    public void postDelayed(Runnable action, long delayMillis) {
      posted.add(action);
      delays.add(delayMillis);
    }

    @Override
    public void removeCallbacks(Runnable action) {
      posted.remove(action);
    }

    @Override
    public void invalidate() {
      invalidations++;
    }

    void elapse() {
      List<Runnable> due = new ArrayList<>(posted);
      posted.clear();
      for (Runnable action : due) {
        action.run();
      }
    }
  }

  private final List<String> mReported = new ArrayList<>();
  private final RecordingHost mHost = new RecordingHost();
  private final FirstFrameDrawGate mGate = new FirstFrameDrawGate(mHost, Build.VERSION_CODES.TIRAMISU);

  private Runnable draw(String name) {
    return () -> mReported.add(name);
  }

  @Test
  public void hasSurfaceNamesOnlyTheSurfaceTheViewHasNow() {
    assertEquals(false, mGate.hasSurface(FIRST_SURFACE));
    mGate.onSurfaceCreated(FIRST_SURFACE);
    assertEquals(true, mGate.hasSurface(FIRST_SURFACE));
    mGate.onSurfaceReleased();
    mGate.onSurfaceCreated(SECOND_SURFACE);
    assertEquals(false, mGate.hasSurface(FIRST_SURFACE));
    assertEquals(true, mGate.hasSurface(SECOND_SURFACE));
    mGate.onSurfaceReleased();
    assertEquals(false, mGate.hasSurface(SECOND_SURFACE));
    assertEquals(false, mGate.hasSurface(0));
  }

  @Test
  public void drawRequestedBeforeTheFirstFrameIsReportedWithIt() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("first"));
    assertEquals(Arrays.asList(), mReported);

    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertEquals(Arrays.asList("first"), mReported);
  }

  @Test
  public void drawRequestedAfterTheFirstFrameIsReportedAtOnce() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onFirstFramePresented(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("resize"));
    assertEquals(Arrays.asList("resize"), mReported);
    assertEquals(0, mHost.posted.size());
  }

  @Test
  public void newSurfaceWaitsForItsOwnFirstFrame() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onFirstFramePresented(FIRST_SURFACE);
    mGate.onSurfaceReleased();
    mGate.onSurfaceCreated(SECOND_SURFACE);
    mGate.onRedrawNeeded(draw("second surface"));
    assertEquals(Arrays.asList(), mReported);

    mGate.onFirstFramePresented(SECOND_SURFACE);
    assertEquals(Arrays.asList("second surface"), mReported);
  }

  @Test
  public void aFrameReportedForAnEarlierSurfaceDoesNotOpenTheNewOne() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onSurfaceReleased();
    mGate.onSurfaceCreated(SECOND_SURFACE);
    mGate.onRedrawNeeded(draw("second surface"));

    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertEquals(Arrays.asList(), mReported);

    mGate.onFirstFramePresented(SECOND_SURFACE);
    assertEquals(Arrays.asList("second surface"), mReported);
  }

  @Test
  public void surfaceReleasedBeforeItsFirstFrameReportsItsPendingDraws() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("a"));
    mGate.onRedrawNeeded(draw("b"));
    mGate.onSurfaceReleased();
    assertEquals(Arrays.asList("a", "b"), mReported);
    assertEquals(0, mHost.posted.size());
  }

  @Test
  public void detachingReportsPendingDrawsOnce() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("held"));
    mGate.onDetachedFromWindow();
    assertEquals(Arrays.asList("held"), mReported);

    mGate.onSurfaceReleased();
    mGate.onFirstFramePresented(FIRST_SURFACE);
    mHost.elapse();
    assertEquals(Arrays.asList("held"), mReported);
  }

  @Test
  public void eachDrawIsReportedOnce() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("once"));
    mGate.onFirstFramePresented(FIRST_SURFACE);
    mGate.onFirstFramePresented(FIRST_SURFACE);
    mGate.onSurfaceReleased();
    mHost.elapse();
    assertEquals(Arrays.asList("once"), mReported);
  }

  @Test
  public void aFirstFrameThatNeverComesIsWaitedForOnlyUntilTheDeadline() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("held"));
    assertEquals(Arrays.asList(Long.valueOf(FirstFrameDrawGate.FIRST_FRAME_WAIT_MILLIS)), mHost.delays);

    mHost.elapse();
    assertEquals(Arrays.asList("held"), mReported);
  }

  @Test
  public void oneDeadlineCoversEveryDrawWaitingForTheSameFrame() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("a"));
    mGate.onRedrawNeeded(draw("b"));
    assertEquals(1, mHost.posted.size());

    mHost.elapse();
    assertEquals(Arrays.asList("a", "b"), mReported);
  }

  @Test
  public void theFirstFrameCancelsTheDeadline() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("framed"));
    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertEquals(0, mHost.posted.size());
    assertEquals(Arrays.asList("framed"), mReported);
  }

  @Test
  public void detachingCancelsTheDeadline() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("held"));
    mGate.onDetachedFromWindow();
    assertEquals(0, mHost.posted.size());
  }

  @Test
  public void fromAndroid14ADrawWaitsForTheFirstFrameWithNoDeadline() {
    FirstFrameDrawGate gate = new FirstFrameDrawGate(mHost, Build.VERSION_CODES.UPSIDE_DOWN_CAKE);
    gate.onSurfaceCreated(FIRST_SURFACE);
    gate.onRedrawNeeded(draw("held"));
    assertEquals(0, mHost.posted.size());

    mHost.elapse();
    assertEquals(Arrays.asList(), mReported);

    gate.onFirstFramePresented(FIRST_SURFACE);
    assertEquals(Arrays.asList("held"), mReported);
  }

  @Test
  public void aDrawAfterTheDeadlineWaitsForTheFirstFrameAgain() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("first"));
    mHost.elapse();
    mGate.onRedrawNeeded(draw("resize"));
    assertEquals(Arrays.asList("first"), mReported);
    assertEquals(1, mHost.posted.size());

    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertEquals(Arrays.asList("first", "resize"), mReported);
  }

  @Test
  public void theHoleOpensWithTheFirstFrame() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    assertFalse(mGate.hasFrame());

    mGate.onFirstFramePresented(FIRST_SURFACE);
    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertTrue(mGate.hasFrame());
    assertEquals(1, mHost.invalidations);
  }

  @Test
  public void aFirstFrameLaterThanTheDeadlineKeepsTheHoleClosedUntilItLands() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onRedrawNeeded(draw("held"));
    mHost.elapse();
    assertEquals(Arrays.asList("held"), mReported);
    assertFalse(mGate.hasFrame());
    assertEquals(0, mHost.invalidations);

    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertTrue(mGate.hasFrame());
    assertEquals(1, mHost.invalidations);
  }

  @Test
  public void aNewSurfaceKeepsTheHoleClosedUntilItsOwnFirstFrame() {
    mGate.onSurfaceCreated(FIRST_SURFACE);
    mGate.onFirstFramePresented(FIRST_SURFACE);
    mGate.onSurfaceReleased();
    assertFalse(mGate.hasFrame());

    mGate.onSurfaceCreated(SECOND_SURFACE);
    mGate.onFirstFramePresented(FIRST_SURFACE);
    assertFalse(mGate.hasFrame());

    mGate.onFirstFramePresented(SECOND_SURFACE);
    assertTrue(mGate.hasFrame());
    assertEquals(3, mHost.invalidations);
  }
}
