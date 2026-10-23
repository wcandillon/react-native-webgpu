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

  void surfaceOffscreen();

  int getContextId();
}
