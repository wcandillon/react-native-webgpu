package com.webgpu;

import android.os.Build;

import androidx.annotation.Nullable;

/** The Android view a WebGPUView draws into, see WebGPUView.updateView(). */
enum BackingViewKind {
  SURFACE_VIEW,
  TEXTURE_VIEW,
  HARDWARE_BUFFER_VIEW;

  /**
   * The kind the androidSurfaceType prop asks for, or null for "auto", which
   * leaves the choice to forAutoSurfaceType. WebGPUHardwareBufferView (a plain
   * View drawing AHardwareBuffers inline) needs Android 10, so below it the
   * request gets a TextureView. It stays opt-in until it falls back to
   * TextureView at runtime when a device cannot import the AHardwareBuffer.
   */
  @Nullable
  static BackingViewKind fromSurfaceType(@Nullable String surfaceType, int sdkInt) {
    if ("SurfaceView".equals(surfaceType)) {
      return SURFACE_VIEW;
    }
    if ("TextureView".equals(surfaceType)) {
      return TEXTURE_VIEW;
    }
    if ("HardwareBufferView".equals(surfaceType)) {
      return sdkInt >= Build.VERSION_CODES.Q ? HARDWARE_BUFFER_VIEW : TEXTURE_VIEW;
    }
    return null;
  }

  /**
   * The kind "auto" picks: a SurfaceView for an opaque canvas and a
   * TextureView otherwise. Before Android 12L an opaque canvas gets a
   * TextureView too, unless it asks for zOrderOnTop, which only a SurfaceView
   * offers: those releases remove a SurfaceView's layer as the view leaves its
   * window, ahead of the window frame that stops showing it, so an opaque
   * canvas leaving the screen leaves a black hole for a frame. Android 8 to 10
   * remove it in onDetachedFromWindow; Android 11 and 12 hand it to the render
   * thread's positionLost, which runs after the detach has cleared the view's
   * root and so applies it at once too. Android 12L applies the removal with
   * the window's next draw.
   */
  static BackingViewKind forAutoSurfaceType(boolean opaque, boolean zOrderOnTop, int sdkInt) {
    if (!opaque) {
      return TEXTURE_VIEW;
    }
    if (sdkInt < Build.VERSION_CODES.S_V2 && !zOrderOnTop) {
      return TEXTURE_VIEW;
    }
    return SURFACE_VIEW;
  }
}
