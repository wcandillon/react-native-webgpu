#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "FramePresenter.h"

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#ifdef __APPLE__
namespace dawn::native::metal {
void WaitForCommandsToBeScheduled(WGPUDevice device);
} // namespace dawn::native::metal
#endif

namespace rnwgpu {

// The portable presentation backend: the canvas context renders into plain
// textures owned by this class, and the UI thread copies the latest finished
// one onto the native surface's swapchain and presents it.
//
// It costs one full-size copy per frame, but it works with any device and any
// texture configuration the surface itself supports, and the wgpu::Surface is
// only ever used from the UI thread (see releaseForDevice for the one
// exception). The copy is submitted on the queue the frame was rendered on, so
// queue order alone guarantees it reads a finished frame: no fence is needed.
//
// Two textures are enough. The rendering thread draws into the back one while
// the UI thread reads the front one; present() swaps them. A frame that was
// never copied before the next one arrives is simply skipped. The front frame
// is kept after it was shown, and disable() hands it back, so it is not lost
// with the surface and can be shown again on the next one.
//
// The view enables the presenter with its native surface and disables it when
// that surface goes away; in between it calls presentFrame() on the UI thread
// whenever the frame-ready callback fired.
class BlitPresenter : public FramePresenter {
public:
  // What the view lends the presenter while it has a native surface.
  struct NativeSurface {
    // Creates a wgpu::Surface for the native window. Called again whenever the
    // surface has to be rebuilt (see resetForDeviceLocked).
    std::function<wgpu::Surface()> create;
    // Optional. Called on the UI thread each time the surface was configured
    // for a format, for platform settings that follow from it (the layer's
    // color space on Apple platforms).
    std::function<void(wgpu::TextureFormat)> configured;
    // Drops the reference the view took on the native window on our behalf.
    std::function<void()> release;
  };

  // What disable() hands back to the caller.
  struct Disabled {
    // To be destroyed outside the caller's lock: it releases a JNI reference.
    std::function<void()> frameReady;
    // The latest finished frame, so it is not lost with the surface. Null when
    // there is none, or when it predates the current configuration (whoever
    // takes it over may render into it again).
    wgpu::Texture lastFrame = nullptr;
  };

  BlitPresenter() = default;
  BlitPresenter(const BlitPresenter &) = delete;
  BlitPresenter &operator=(const BlitPresenter &) = delete;

  ~BlitPresenter() override { disable(); }

  // --- Mode (UI thread)
  // -------------------------------------------------------

  bool isEnabled() const override { return _enabled.load(); }

  // The view has a native surface: frames go to it from now on.
  void enable(NativeSurface nativeSurface) {
    std::function<void()> releasePrevious;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      // Enabled again without a disable in between: the new surface replaces
      // the previous one.
      releasePrevious = detachSurfaceLocked();
      _nativeSurface = std::move(nativeSurface);
      _enabled.store(true);
      // Whatever frame we hold has not been shown on this surface.
      _dirty = _front.texture != nullptr;
    }
    if (releasePrevious) {
      releasePrevious();
    }
  }

  // The native surface is going away. Blocks until a copy in progress on it
  // is done, so the surface is no longer in use when this returns.
  Disabled disable() {
    Disabled result;
    std::function<void()> release;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _enabled.store(false);
      release = detachSurfaceLocked();
      result.frameReady = std::exchange(_frameReady, nullptr);
      if (_front.generation == _generation) {
        result.lastFrame = std::move(_front.texture);
      }
      _front = {};
      // A frame in flight is abandoned; its texture stays alive for as long as
      // the rendering thread holds it.
      _back = {};
      _frameOpen = false;
      _dirty = false;
    }
    if (release) {
      release();
    }
    return result;
  }

  // Take over a finished frame that was rendered offscreen while this
  // presenter was not active, so it shows up without waiting for the next
  // render. The texture must belong to the configured device, match the
  // current configuration and have CopySrc usage. Returns false when it was
  // not taken.
  bool adoptFrame(const wgpu::Texture &frame) {
    std::function<void()> frameReady;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_enabled.load() || _device == nullptr || frame == nullptr) {
        return false;
      }
      _front = {frame, _generation};
      _dirty = true;
      frameReady = _frameReady;
    }
    if (frameReady) {
      frameReady();
    }
    return true;
  }

  // --- Rendering thread (FramePresenter)
  // --------------------------------------

  void configure(const wgpu::SurfaceConfiguration &config) override {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_device.Get() != config.device.Get()) {
      // The frames and the swapchain belong to the previous device.
      resetForDeviceLocked();
    }
    _device = config.device;
    _format = config.format;
    _usage = config.usage;
    _alphaMode = config.alphaMode;
    _viewFormats.assign(config.viewFormats,
                        config.viewFormats + config.viewFormatCount);
    // Textures created before this point no longer match the configuration.
    // The front one is still fine to show; it is just never rendered into
    // again.
    _generation++;
    _back = {};
    _frameOpen = false;
    _warnedUnpresentable = false;
  }

  void unconfigure() override {
    std::lock_guard<std::mutex> lock(_mutex);
    resetForDeviceLocked();
    _device = nullptr;
    _generation++;
  }

  // Runs on whichever thread destroys the device, not on the UI thread: the
  // one place where the surface is touched from another thread, under _mutex.
  void releaseForDevice(const wgpu::Device &device) override {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_device && _device.Get() == device.Get()) {
      resetForDeviceLocked();
      _device = nullptr;
    }
  }

  void resize(int width, int height) override {
    std::lock_guard<std::mutex> lock(_mutex);
    if (width == _width && height == _height) {
      return;
    }
    _width = width;
    _height = height;
    // A frame in flight was acquired at the previous size: it is abandoned.
    _frameOpen = false;
  }

  wgpu::Texture getCurrentTexture() override {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_enabled.load() || _device == nullptr || _width <= 0 || _height <= 0) {
      return nullptr;
    }
    if (_frameOpen) {
      return _back.texture;
    }
    if (!_back.texture || _back.generation != _generation ||
        _back.texture.GetWidth() != static_cast<uint32_t>(_width) ||
        _back.texture.GetHeight() != static_cast<uint32_t>(_height)) {
      _back = {createTextureLocked(), _generation};
    }
    _frameOpen = true;
    return _back.texture;
  }

  void present() override {
    std::function<void()> frameReady;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_frameOpen) {
        return; // nothing was acquired since the last present
      }
      _frameOpen = false;
      std::swap(_front, _back);
      _dirty = true;
      frameReady = _frameReady;
    }
    if (frameReady) {
      frameReady(); // outside the lock: it calls into the platform
    }
  }

  // --- View (UI thread)
  // -------------------------------------------------------

  // Copy the latest finished frame onto the native surface and present it, if
  // it is not on screen yet. Returns true when a buffer was queued for
  // display. The caller must not call this again before the platform has
  // consumed that buffer, or acquiring the next one may block the UI thread.
  bool presentFrame() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_dirty || !_front.texture || _device == nullptr ||
        !_nativeSurface.create) {
      return false;
    }
    const wgpu::Texture &frame = _front.texture;
    wgpu::Texture target = acquireSurfaceTextureLocked(frame);
    if (!target) {
      // Still dirty: retried on the next wake-up.
      return false;
    }

    wgpu::TexelCopyTextureInfo source = {};
    source.texture = frame;
    wgpu::TexelCopyTextureInfo destination = {};
    destination.texture = target;
    // The swapchain is configured to the frame size, but the platform has the
    // last word on its extent; copy the shared region.
    wgpu::Extent3D size = {std::min(frame.GetWidth(), target.GetWidth()),
                           std::min(frame.GetHeight(), target.GetHeight()), 1};

    wgpu::CommandEncoderDescriptor encoderDescriptor;
    wgpu::CommandEncoder encoder =
        _device.CreateCommandEncoder(&encoderDescriptor);
    encoder.CopyTextureToTexture(&source, &destination, &size);
    wgpu::CommandBuffer commands = encoder.Finish();
    _device.GetQueue().Submit(1, &commands);
#ifdef __APPLE__
    // Metal presents the drawable right away: the copy has to be scheduled
    // first.
    dawn::native::metal::WaitForCommandsToBeScheduled(_device.Get());
#endif
    _surface.Present();
    _dirty = false;
    return true;
  }

  // Invoked from the rendering thread (never under _mutex) each time a new
  // frame is finished, so the view can wake up and call presentFrame() on the
  // UI thread. Pass nullptr to unregister.
  void setFrameReadyCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(_mutex);
    _frameReady = std::move(cb);
  }

private:
  // All *Locked helpers below require _mutex to be held.

  struct Frame {
    wgpu::Texture texture = nullptr;
    // The configuration generation the texture was created for.
    uint64_t generation = 0;
  };

  wgpu::Texture createTextureLocked() {
    wgpu::TextureDescriptor descriptor;
    // Union with the configured usage so frames stay compatible with whatever
    // the app asked for (e.g. CopySrc readbacks). RenderAttachment and CopySrc
    // are what the presenter itself needs; TextureBinding keeps the texture
    // identical to SurfaceInfo's offscreen one, which adoptFrame() takes over.
    descriptor.usage = _usage | wgpu::TextureUsage::RenderAttachment |
                       wgpu::TextureUsage::CopySrc |
                       wgpu::TextureUsage::TextureBinding;
    descriptor.format = _format;
    descriptor.size.width = static_cast<uint32_t>(_width);
    descriptor.size.height = static_cast<uint32_t>(_height);
    descriptor.viewFormats =
        _viewFormats.empty() ? nullptr : _viewFormats.data();
    descriptor.viewFormatCount = _viewFormats.size();
    return _device.CreateTexture(&descriptor);
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Drop everything that references _device while it is still alive. That
  // includes the surface: Dawn keeps a swapchain for the device inside it even
  // after Unconfigure(), which must not outlive the device, and on Vulkan a
  // surface cannot be switched to another device anyway. It is rebuilt from
  // the native window by the next presentFrame(). See
  // SurfaceInfo::releaseSurfaceForDevice for the full story.
  void resetForDeviceLocked() {
    _surface = nullptr;
    _surfaceConfigured = false;
    _front = {};
    _back = {};
    _frameOpen = false;
    _dirty = false;
  }

  // Drops the surface and returns the closure releasing the native window, to
  // be called outside the lock.
  std::function<void()> detachSurfaceLocked() {
    _surface = nullptr;
    _surfaceConfigured = false;
    auto release = std::move(_nativeSurface.release);
    _nativeSurface = {};
    return release;
  }

  // The swapchain texture to copy `frame` into, with the surface (re)built and
  // (re)configured for it as needed. Null when the surface cannot take it.
  wgpu::Texture acquireSurfaceTextureLocked(const wgpu::Texture &frame) {
    if (!_surface) {
      _surface = _nativeSurface.create();
      _surfaceConfigured = false;
      if (!_surface) {
        return nullptr;
      }
    }
    wgpu::SurfaceConfiguration config;
    config.device = _device;
    config.format = frame.GetFormat();
    config.width = frame.GetWidth();
    config.height = frame.GetHeight();
    config.usage =
        wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = _alphaMode;
    if (!_surfaceConfigured || _surfaceFormat != config.format ||
        _surfaceWidth != config.width || _surfaceHeight != config.height) {
      if (!canPresentLocked(config.format)) {
        return nullptr;
      }
      _surface.Configure(&config);
      _surfaceConfigured = true;
      _surfaceFormat = config.format;
      _surfaceWidth = config.width;
      _surfaceHeight = config.height;
      if (_nativeSurface.configured) {
        _nativeSurface.configured(config.format);
      }
    }
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (!isAcquireSuccess(surfaceTexture)) {
      if (surfaceTexture.status ==
          wgpu::SurfaceGetCurrentTextureStatus::Error) {
        return nullptr;
      }
      // The surface is stale (rotation, resize, coming back from background):
      // reconfigure once.
      _surface.Configure(&config);
      _surface.GetCurrentTexture(&surfaceTexture);
      if (!isAcquireSuccess(surfaceTexture)) {
        return nullptr;
      }
    }
    return surfaceTexture.texture;
  }

  static bool isAcquireSuccess(const wgpu::SurfaceTexture &surfaceTexture) {
    return (surfaceTexture.status ==
                wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
            surfaceTexture.status ==
                wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) &&
           surfaceTexture.texture != nullptr;
  }

  // Whether the surface can be the destination of the copy for this format.
  // Reported once per configuration instead of as a validation error on every
  // frame.
  bool canPresentLocked(wgpu::TextureFormat format) {
    wgpu::SurfaceCapabilities capabilities;
    if (_surface.GetCapabilities(_device.GetAdapter(), &capabilities) !=
        wgpu::Status::Success) {
      return true; // unknown: let Configure report it
    }
    bool formatSupported = false;
    for (size_t i = 0; i < capabilities.formatCount; i++) {
      if (capabilities.formats[i] == format) {
        formatSupported = true;
        break;
      }
    }
    bool copySupported =
        (capabilities.usages & wgpu::TextureUsage::CopyDst) != 0;
    if (formatSupported && copySupported) {
      return true;
    }
    if (!_warnedUnpresentable) {
      _warnedUnpresentable = true;
      warn(formatSupported
               ? "the native surface cannot be the destination of a copy; "
                 "the canvas stays blank"
               : "the configured canvas format is not supported by the native "
                 "surface; the canvas stays blank (use "
                 "navigator.gpu.getPreferredCanvasFormat())");
    }
    return false;
  }

  // Failure reporting only (never per-frame).
  static void warn(const char *message) {
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_WARN, "WebGPUBlitPresenter", "%s", message);
#else
    fprintf(stderr, "[WebGPU] BlitPresenter: %s\n", message);
#endif
  }

  std::atomic<bool> _enabled{false};
  std::mutex _mutex;

  // The canvas configuration. device == nullptr means "not configured".
  // Declared before the Dawn objects created from it so it outlives them.
  wgpu::Device _device = nullptr;
  wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
  wgpu::TextureUsage _usage = wgpu::TextureUsage::RenderAttachment;
  wgpu::CompositeAlphaMode _alphaMode = wgpu::CompositeAlphaMode::Auto;
  std::vector<wgpu::TextureFormat> _viewFormats;
  // Bumped whenever the configuration changes.
  uint64_t _generation = 0;
  // Drawing buffer size (px) of the next frame.
  int _width = 0;
  int _height = 0;

  // The frame being rendered (while _frameOpen), or a texture to reuse for the
  // next one.
  Frame _back;
  // The latest finished frame.
  Frame _front;
  // Set by getCurrentTexture(), cleared by present().
  bool _frameOpen = false;
  // _front has not been copied onto the current surface yet.
  bool _dirty = false;

  // The native surface lent by the view, and the Dawn surface built from it
  // (null until the first presentFrame(), and after resetForDeviceLocked()).
  NativeSurface _nativeSurface;
  wgpu::Surface _surface = nullptr;
  // What _surface is currently configured for.
  bool _surfaceConfigured = false;
  wgpu::TextureFormat _surfaceFormat = wgpu::TextureFormat::Undefined;
  uint32_t _surfaceWidth = 0;
  uint32_t _surfaceHeight = 0;
  bool _warnedUnpresentable = false;

  std::function<void()> _frameReady; // view wake-up, see setter
};

} // namespace rnwgpu
