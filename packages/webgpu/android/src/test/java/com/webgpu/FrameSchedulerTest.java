package com.webgpu;

import static org.junit.Assert.assertEquals;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.junit.Test;

/**
 * Drives FrameScheduler through a host that records what it is asked to do and asserts the exact
 * sequence, for both backing views.
 */
public class FrameSchedulerTest {

  private static final String FRAME_CALLBACK = "frameCallback";
  private static final String POST_BEHIND_PENDING_DRAW = "postBehindPendingDraw";
  private static final String PRESENT = "present";

  private static final class RecordingHost implements FrameScheduler.Host {
    final List<String> calls = new ArrayList<>();
    // What presentFrame() reports: whether a frame is still waiting.
    boolean frameLeft;

    @Override
    public void postFrameCallback() {
      calls.add(FRAME_CALLBACK);
    }

    @Override
    public void postBehindPendingDraw() {
      calls.add(POST_BEHIND_PENDING_DRAW);
    }

    @Override
    public boolean presentFrame() {
      calls.add(PRESENT);
      return frameLeft;
    }

    List<String> drain() {
      List<String> copy = new ArrayList<>(calls);
      calls.clear();
      return copy;
    }
  }

  private static List<String> seq(String... calls) {
    return Arrays.asList(calls);
  }

  // --- TextureView -----------------------------------------------------------

  @Test
  public void textureViewFrameIsPostedBehindThePendingDrawAndPresentedThere() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.TEXTURE_VIEW);
    scheduler.requestFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
    scheduler.onPosted();
    assertEquals(seq(PRESENT), host.drain());
  }

  @Test
  public void textureViewRequestsWaitingForOnePostArePostedOnce() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.TEXTURE_VIEW);
    scheduler.requestFrame();
    scheduler.requestFrame();
    scheduler.requestFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
    scheduler.onPosted();
    assertEquals(seq(PRESENT), host.drain());
    // The post was consumed: the next request posts again.
    scheduler.requestFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
  }

  @Test
  public void textureViewLeftoverIsRetriedOnTheNextVsyncBehindThePendingDraw() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.TEXTURE_VIEW);
    scheduler.requestFrame();
    host.drain();
    host.frameLeft = true;
    scheduler.onPosted();
    // Not retried straight away: on the next vsync.
    assertEquals(seq(PRESENT, FRAME_CALLBACK), host.drain());
    host.frameLeft = false;
    // A TextureView frame is never presented inside the Choreographer frame.
    scheduler.onFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
    scheduler.onPosted();
    assertEquals(seq(PRESENT), host.drain());
  }

  @Test
  public void textureViewRequestMadeWhileARetryIsOutstandingWaitsForIt() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.TEXTURE_VIEW);
    scheduler.requestFrame();
    host.drain();
    host.frameLeft = true;
    scheduler.onPosted();
    assertEquals(seq(PRESENT, FRAME_CALLBACK), host.drain());
    host.frameLeft = false;
    scheduler.requestFrame();
    assertEquals(seq(), host.drain());
    scheduler.onFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
    scheduler.onPosted();
    assertEquals(seq(PRESENT), host.drain());
  }

  // --- SurfaceView -----------------------------------------------------------

  @Test
  public void surfaceViewFrameIsPresentedOnTheVsync() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.SURFACE_VIEW);
    scheduler.requestFrame();
    assertEquals(seq(FRAME_CALLBACK), host.drain());
    scheduler.onFrame();
    assertEquals(seq(PRESENT), host.drain());
  }

  @Test
  public void surfaceViewRequestsWaitingForOneVsyncArePostedOnce() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.SURFACE_VIEW);
    scheduler.requestFrame();
    scheduler.requestFrame();
    assertEquals(seq(FRAME_CALLBACK), host.drain());
    scheduler.onFrame();
    assertEquals(seq(PRESENT), host.drain());
    scheduler.requestFrame();
    assertEquals(seq(FRAME_CALLBACK), host.drain());
  }

  @Test
  public void surfaceViewLeftoverIsRetriedOnTheNextVsyncNotStraightAway() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.SURFACE_VIEW);
    scheduler.requestFrame();
    host.drain();
    host.frameLeft = true;
    scheduler.onFrame();
    assertEquals(seq(PRESENT, FRAME_CALLBACK), host.drain());
    host.frameLeft = false;
    scheduler.onFrame();
    assertEquals(seq(PRESENT), host.drain());
  }

  @Test
  public void surfaceViewRequestMadeWhileARetryIsOutstandingWaitsForIt() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.SURFACE_VIEW);
    scheduler.requestFrame();
    host.drain();
    host.frameLeft = true;
    scheduler.onFrame();
    host.drain();
    host.frameLeft = false;
    scheduler.requestFrame();
    assertEquals(seq(), host.drain());
    scheduler.onFrame();
    assertEquals(seq(PRESENT), host.drain());
  }

  // --- Cancel ----------------------------------------------------------------

  @Test
  public void cancelDropsTheOutstandingCallbacks() {
    RecordingHost host = new RecordingHost();
    FrameScheduler scheduler = new FrameScheduler(host, FrameScheduler.Kind.SURFACE_VIEW);
    scheduler.requestFrame();
    assertEquals(seq(FRAME_CALLBACK), host.drain());
    scheduler.cancel();
    // The host removed the callback; a new request has to post again.
    scheduler.requestFrame();
    assertEquals(seq(FRAME_CALLBACK), host.drain());

    FrameScheduler textureScheduler =
        new FrameScheduler(host, FrameScheduler.Kind.TEXTURE_VIEW);
    textureScheduler.requestFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
    textureScheduler.cancel();
    textureScheduler.requestFrame();
    assertEquals(seq(POST_BEHIND_PENDING_DRAW), host.drain());
  }
}
