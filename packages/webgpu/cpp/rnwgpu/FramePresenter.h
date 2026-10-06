#pragma once

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// The rendering-thread side of a presentation backend that takes finished
// frames from a canvas context and puts them on screen from the UI thread.
//
// The canvas context renders each frame into a texture the backend owns
// (getCurrentTexture) and hands it over when the frame is done (present). How
// the frame then reaches the screen is the backend's business and happens on
// the UI thread, driven by the native view: the wgpu::Surface, if there is
// one, never leaves that thread.
//
// SurfaceInfo drives this interface, from whichever thread renders, while the
// backend is the context's presentation mode. The view talks to the concrete
// backend for its own side (attaching, consuming frames).
//
// Implementations guard their state with their own mutex. SurfaceInfo may call
// in while holding its mutex (lock order SurfaceInfo -> presenter), except for
// getCurrentTexture(), which is allowed to block.
class FramePresenter {
public:
  virtual ~FramePresenter() = default;

  // True while the backend is the context's presentation mode.
  virtual bool isEnabled() const = 0;

  // The canvas context was (re)configured. Only the device, format, usage,
  // view formats and alpha mode are read; the drawing buffer size comes from
  // resize(). The view formats are copied.
  virtual void configure(const wgpu::SurfaceConfiguration &config) = 0;

  virtual void unconfigure() = 0;

  // Called right before `device` is destroyed: drop everything that belongs to
  // it while it is still alive.
  virtual void releaseForDevice(const wgpu::Device &device) = 0;

  // The drawing buffer size (px) of the next frame. Called before every
  // getCurrentTexture().
  virtual void resize(int width, int height) = 0;

  // The texture to render the current frame into: the same one until
  // present(). Null when the backend cannot take a frame right now; the
  // caller then renders offscreen and the frame is not shown.
  virtual wgpu::Texture getCurrentTexture() = 0;

  // The current frame is finished: hand it to the view.
  virtual void present() = 0;
};

} // namespace rnwgpu
