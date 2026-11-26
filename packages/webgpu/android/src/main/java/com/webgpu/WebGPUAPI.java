package com.webgpu;

import android.view.Surface;
import android.view.View;

import com.facebook.proguard.annotations.DoNotStrip;

/**
 * Surface lifecycle events a WebGPU child view reports. The registry entry
 * itself is owned by the JS Canvas component (created lazily, removed via
 * RNWebGPU.destroyContext on unmount); views only attach and detach surfaces.
 * A detached context keeps rendering into an offscreen texture whose content
 * is blitted onto the next attached surface. A parked one keeps its surface
 * for when the view is back (see WebGPUTextureView).
 */
public interface WebGPUAPI {

  void surfaceCreated(
    Surface surface
  );

  void surfaceChanged(
    Surface surface
  );

  void surfaceOffscreen();

  // The view left its window but kept its surface: keep it, rendering
  // offscreen, until surfaceUnparked().
  void surfaceParked();

  // The view is back in a window with the surface it kept. False when the
  // native side gave that surface up while the view was away.
  boolean surfaceUnparked();

  // The native side gave up the surface this backing view kept (see
  // surfaceUnparked()): replace the view with a new one, which gets a new
  // window the usual way.
  void backingViewLost(View view);
}
