package com.webgpu;

import androidx.annotation.Nullable;

/**
 * The Android view a WebGPUView draws into, see WebGPUView.updateView(), and
 * the rules the props resolve by. The JS side (Canvas.tsx) mirrors them to
 * warn in development.
 */
enum BackingViewKind {
  SURFACE_VIEW,
  TEXTURE_VIEW,
  HARDWARE_BUFFER_VIEW;

  /**
   * The kind the androidBackingView prop names; a TextureView by default. A
   * HardwareBufferView (a plain View drawing AHardwareBuffers inline) needs
   * Android 10: below it, the request falls back to a TextureView, which
   * composites the same way, and the JS side warns about it in development.
   */
  static BackingViewKind fromBackingView(@Nullable String backingView, boolean hardwareBufferSupported) {
    if ("SurfaceView".equals(backingView)) {
      return SURFACE_VIEW;
    }
    if ("HardwareBufferView".equals(backingView) && hardwareBufferSupported) {
      return HARDWARE_BUFFER_VIEW;
    }
    return TEXTURE_VIEW;
  }

  /**
   * Whether the canvas composites as opaque: what the prop asks for, except
   * that a HardwareBufferView always composites with alpha.
   */
  static boolean resolveOpaque(BackingViewKind kind, boolean opaque) {
    return opaque && kind != HARDWARE_BUFFER_VIEW;
  }
}
