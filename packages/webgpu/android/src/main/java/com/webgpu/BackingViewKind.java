package com.webgpu;

import android.os.Build;

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

  /**
   * Whether a changed zOrderOnTop moves a mounted SurfaceView: SurfaceView
   * applies it to its live surface from Android 11, for an app targeting it.
   * Before that, the surface reads it when it is created, and so does a media
   * overlay on Android 13, whose SurfaceView moves a surface leaving the top
   * to the default sublayer at once, before setZOrderMediaOverlay() can ask
   * for its own: it would come back among the other SurfaceViews below the
   * window rather than above them.
   */
  static boolean zOrderOnTopChangesInPlace(int sdk, int targetSdk, boolean mediaOverlay) {
    if (sdk < Build.VERSION_CODES.R || targetSdk < Build.VERSION_CODES.R) {
      return false;
    }
    return !(mediaOverlay && sdk == Build.VERSION_CODES.TIRAMISU);
  }

  /**
   * Whether a changed zOrderMediaOverlay moves a mounted SurfaceView: only
   * from Android 14, whose SurfaceView compares the sublayer it is asked for
   * with the one its surface has. Up to Android 13, setZOrderMediaOverlay()
   * only sets the sublayer the next surface is created with.
   */
  static boolean zOrderMediaOverlayChangesInPlace(int sdk) {
    return sdk >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
  }
}
