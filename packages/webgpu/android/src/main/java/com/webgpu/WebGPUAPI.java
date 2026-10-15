package com.webgpu;

import android.view.Surface;

import com.facebook.proguard.annotations.DoNotStrip;

/**
 * Surface lifecycle events a WebGPU child view reports. The registry entry
 * itself is owned by the JS Canvas component (created lazily, removed via
 * RNWebGPU.destroyContext on unmount); views only attach and detach surfaces.
 * A detached context keeps rendering into an offscreen texture whose content
 * is blitted onto the next attached surface.
 */
public interface WebGPUAPI {
  /** What a backing view the canvas no longer draws into reports to: nothing. */
  WebGPUAPI DETACHED = new WebGPUAPI() {
    @Override
    public void surfaceCreated(Surface surface, long firstFrameToken, boolean beforeFirstDraw) {}

    @Override
    public void surfaceChanged(Surface surface) {}

    @Override
    public void keepFrame(Surface surface) {}

    @Override
    public void surfaceOffscreen() {}

    @Override
    public void surfaceUpdated() {}

    @Override
    public int getContextId() {
      return 0;
    }
  };

  /**
   * firstFrameToken is nonzero for a surface whose view waits for its first
   * frame (a SurfaceView behind its window, see FirstFrameDrawGate): the
   * native side reports that frame back with it, from the thread that
   * presented it, through WebGPUView.onFirstFramePresented. beforeFirstDraw
   * is set for a surface the view was given before its first draw, which
   * shows the latest frame only if it is presented into it right away.
   */
  void surfaceCreated(
    Surface surface,
    long firstFrameToken,
    boolean beforeFirstDraw
  );

  void surfaceChanged(
    Surface surface
  );

  /**
   * The surface is about to be destroyed together with the frame it shows: the
   * canvas reads that frame back and shows it on its next surface. Main
   * thread, before surfaceOffscreen, while the surface is still valid.
   */
  void keepFrame(Surface surface);

  void surfaceOffscreen();

  /** A TextureView's window is drawing the newest frame of its surface. */
  void surfaceUpdated();

  int getContextId();
}
