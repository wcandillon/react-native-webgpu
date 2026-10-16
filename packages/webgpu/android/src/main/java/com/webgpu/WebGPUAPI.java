package com.webgpu;

import android.view.Surface;

import com.facebook.proguard.annotations.DoNotStrip;

/**
 * Surface lifecycle events a WebGPU child view reports. The registry entry
 * itself is owned by the JS Canvas component (created lazily, removed via
 * RNWebGPU.destroyContext on unmount); views only attach and detach surfaces,
 * and suspend and resume a surface they keep while out of their window. A
 * detached or suspended context keeps rendering into an offscreen texture
 * whose content is blitted onto the next attached or resumed surface.
 */
public interface WebGPUAPI {

  /**
   * firstFrameToken is nonzero for a surface whose view waits for its first
   * frame (a SurfaceView behind its window, see FirstFrameDrawGate): the
   * native side reports that frame back with it, from the thread that
   * presented it, through WebGPUView.onFirstFramePresented.
   */
  void surfaceCreated(
    Surface surface,
    long firstFrameToken
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

  /**
   * The view left its window and keeps its surface, connected, for its
   * return: the canvas presents nothing into it, so it keeps its last frame.
   */
  void surfaceSuspended();

  /** The view is back in its window with the surface it kept. */
  void surfaceResumed();

  int getContextId();
}
