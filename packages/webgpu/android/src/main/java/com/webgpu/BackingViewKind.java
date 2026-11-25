package com.webgpu;

import androidx.annotation.Nullable;

/**
 * The Android view a WebGPUView draws into, see WebGPUView.updateView(), and
 * the rule the prop resolves by. The JS side (Canvas.tsx) mirrors it to warn
 * in development.
 */
enum BackingViewKind {
  SURFACE_VIEW,
  TEXTURE_VIEW;

  /** The kind the androidBackingView prop names; a TextureView by default. */
  static BackingViewKind fromBackingView(@Nullable String backingView) {
    if ("SurfaceView".equals(backingView)) {
      return SURFACE_VIEW;
    }
    return TEXTURE_VIEW;
  }
}
