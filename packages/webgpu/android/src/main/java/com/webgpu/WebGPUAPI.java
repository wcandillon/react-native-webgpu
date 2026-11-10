package com.webgpu;

import android.graphics.SurfaceTexture;
import android.view.Surface;

/**
 * Surface lifecycle events a WebGPU child view reports, on the main thread.
 * The registry entry itself is owned by the JS Canvas component (created
 * lazily, removed via RNWebGPU.destroyContext on unmount); views only attach
 * and detach windows, on the present thread (see WebGPUView). A detached
 * context keeps rendering into its frames, which the next window shows.
 */
public interface WebGPUAPI {
  void onSurfaceCreated(Surface surface, int width, int height);

  void onSurfaceChanged(Surface surface, int width, int height);

  void onSurfaceTextureCreated(SurfaceTexture surface, int width, int height);

  void onSurfaceTextureChanged(SurfaceTexture surface, int width, int height);

  /** The TextureView drew the frame it was given (see rnsurface::FrameScheduler::onFrameConsumed). */
  void onSurfaceTextureUpdated();

  /**
   * The TextureView lets go of its SurfaceTexture: whether it may release it now, else
   * the view does (see TextureView.SurfaceTextureListener.onSurfaceTextureDestroyed).
   */
  boolean onSurfaceTextureDestroyed(SurfaceTexture surface);

  /**
   * The SurfaceView's surface is about to be destroyed. Called once more as
   * the view leaves the window, after the surface went with it.
   */
  void onSurfaceDestroyed();

  int getContextId();
}
