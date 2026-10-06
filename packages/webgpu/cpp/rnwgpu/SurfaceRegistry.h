#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "BlitPresenter.h"
#include "FramePresenter.h"

#if defined(__ANDROID__)
#include "HardwareBufferPresenter.h"
#endif

#ifdef __APPLE__
namespace dawn::native::metal {
void WaitForCommandsToBeScheduled(WGPUDevice device);
} // namespace dawn::native::metal
#endif

namespace rnwgpu {

#ifdef __APPLE__
// Tags the CAMetalLayer with the colorspace matching the configured texture
// format. Implemented in apple/MetalLayerColorSpace.mm.
void applyCAMetalLayerColorSpace(void *nativeSurface,
                                 wgpu::TextureFormat format);
#endif

struct NativeInfo {
  void *nativeSurface;
  int width;
  int height;
};

struct Size {
  int width;
  int height;
};

// Invoked with the platform's native surface pointer once SurfaceInfo is done
// with it, so the platform can drop the reference it acquired on our behalf
// (ANativeWindow_release on Android, CFBridgingRelease of the retained
// CAMetalLayer on Apple platforms). May run on any thread.
using NativeSurfaceReleaser = std::function<void(void *)>;

// Bridges the asynchronous native surface lifecycle (surfaces appear and
// disappear on the platform UI thread) with the synchronous WebGPU canvas API
// (the JS render loop must always be able to acquire a texture).
//
// Ownership & threading model:
// - A registry entry is created on first use (by whichever of JS/native gets
//   there first) and lives exactly as long as its JS Canvas: contextIds are
//   never reused. It is removed by the native view's teardown (MetalView
//   dealloc / WebGPUViewManager.onDropViewInstance) when a surface is
//   attached, or by RNWebGPU.destroyContext (the Canvas unmount cleanup) when
//   none ever was — see RNWebGPU::destroyContext for why the split. Surface
//   destruction alone (backgrounding, TextureView teardown) never removes an
//   entry; it only detaches the surface.
// - Attaching a surface is LATCHED: the UI thread stores it as pending
//   (attachSurface) and it is adopted at the next frame boundary — start of
//   getCurrentTexture or end of presentFrame — on whichever thread renders
//   (main JS, Reanimated UI, or a worklet runtime). This preserves Dawn
//   surface thread-affinity and guarantees a surface is never swapped in the
//   middle of a frame. For contexts that are not actively rendering,
//   RNWebGPUManager::flushPendingSurfaceTransition applies the attach from the
//   JS thread instead.
// - Detaching (switchToOffscreen) is IMMEDIATE, because the platform destroys
//   the surface as soon as its callback returns. A configured context falls
//   back to rendering into an offscreen texture, so a running render loop
//   keeps working; the in-flight frame, if any, is dropped at present(). When
//   a new surface attaches, the latest offscreen frame is blitted onto it so
//   content appears without waiting for the next render — the same mechanism
//   that gives a fast time-to-first-frame when rendering starts before the
//   native surface exists.
class SurfaceInfo {
public:
  SurfaceInfo(wgpu::Instance gpu, int width, int height)
      : _gpu(std::move(gpu)), _width(width), _height(height) {}

  ~SurfaceInfo() {
    // Free the presenters' frames (and stop the fence waiter) before anything
    // else goes away.
#if defined(__ANDROID__)
    _hardwareBufferPresenter.shutdown();
#endif
    _blitPresenter.disable();
    // Drop the Dawn objects before releasing the native surfaces they borrow.
    _surface = nullptr;
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = nullptr;
#endif
    _pendingSurface = nullptr;
    _texture = nullptr;
    if (_pendingReleaser && _pendingNativeSurface) {
      _pendingReleaser(_pendingNativeSurface);
    }
    if (_releaser && _nativeSurface) {
      _releaser(_nativeSurface);
    }
  }

  // --- Platform UI thread ---------------------------------------------------

  // Store a newly created on-screen surface. It becomes active at the next
  // frame boundary (applyPendingAttach); callers should follow up with
  // RNWebGPUManager::flushPendingSurfaceTransition so contexts that are not
  // currently rendering also pick it up.
  void attachSurface(void *nativeSurface, wgpu::Surface surface,
                     NativeSurfaceReleaser releaser) {
    void *replacedSurface = nullptr;
    NativeSurfaceReleaser replacedReleaser;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_hasPendingAttach) {
        // Replaced before it was ever adopted.
        replacedSurface = _pendingNativeSurface;
        replacedReleaser = std::move(_pendingReleaser);
      }
      _hasPendingAttach = true;
      _pendingNativeSurface = nativeSurface;
      _pendingSurface = std::move(surface);
      _pendingReleaser = std::move(releaser);
    }
    if (replacedReleaser && replacedSurface) {
      replacedReleaser(replacedSurface);
    }
  }

  // The platform surface is being destroyed: detach immediately. If the
  // context is configured, rendering continues into an offscreen texture whose
  // content is blitted to the next attached surface; present() no-ops until
  // then. Safe to call when already offscreen.
  void switchToOffscreen() { detach(/* createFallbackTexture = */ true); }

  // Detach without creating the offscreen fallback: used when the context is
  // being destroyed and nothing will consume further frames.
  void detachSurface() { detach(/* createFallbackTexture = */ false); }

  // Reflects native view layout changes. Does not resize the drawing buffer:
  // that tracks canvas.width/height (like on the web), see
  // GPUCanvasContext::getCurrentTexture.
  void resize(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _width = newWidth;
    _height = newHeight;
  }

  // --- Frame boundary (rendering thread, or the JS thread via
  // RNWebGPUManager::flushPendingSurfaceTransition) ---------------------------

  // Adopt a pending surface if no frame is in flight: configure it and, if
  // frames were rendered offscreen, blit the most recent one onto it and
  // present it, so content shows up without waiting for the render loop.
  // Safe to call from any thread; no-ops when there is nothing pending.
  //
  // supersedeInFlightFrame is set by the rendering thread when it starts a new
  // frame: a previous frame that never presented is abandoned and must not
  // block adoption. The flush path (other threads) leaves it false so it never
  // swaps the surface under a frame that is genuinely in flight.
  void applyPendingAttach(bool supersedeInFlightFrame = false) {
    bool presentBlit = false;
    uint64_t blitEpoch = 0;
    wgpu::Device device = nullptr;
    void *replacedSurface = nullptr;
    NativeSurfaceReleaser replacedReleaser;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (supersedeInFlightFrame) {
        _frameInFlight = false;
        _acquiredFromSurface = false;
      }
      if (!_hasPendingAttach || _frameInFlight) {
        return;
      }
      // Attach over attach without a detach in between: replace. Ownership
      // tracks the native window pointer, not the Dawn surface handle (which
      // can be null if surface creation failed).
      replacedSurface = _nativeSurface;
      replacedReleaser = std::move(_releaser);
      _surface = std::move(_pendingSurface);
#if defined(__ANDROID__)
      // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
      _surfaceDevice = nullptr;
#endif
      _nativeSurface = _pendingNativeSurface;
      _releaser = std::move(_pendingReleaser);
      _hasPendingAttach = false;
      _pendingNativeSurface = nullptr;
      _frameEpoch++;

      // _surface can be null here when Dawn surface creation failed for a
      // valid native window; the context then just keeps rendering offscreen.
      if (_config.device != nullptr && _surface) {
        bool blit = _texture != nullptr;
        // The blit needs CopyDst on the surface. Configure with a widened
        // copy while keeping _config at the usage the user asked for, so any
        // later reconfigure drops the extra flag again.
        wgpu::SurfaceConfiguration config = _config;
        if (blit) {
          config.usage |= wgpu::TextureUsage::CopyDst;
        }
        configureSurfaceLocked(config);
#ifdef __APPLE__
        applyCAMetalLayerColorSpace(_nativeSurface, _config.format);
#endif
        if (blit) {
          presentBlit = blitOffscreenToSurfaceLocked();
          device = _config.device;
          // Consumed either way; on failure the next frame renders fresh.
          _texture = nullptr;
          blitEpoch = _frameEpoch;
        }
      }
    }
    if (replacedReleaser && replacedSurface) {
      replacedReleaser(replacedSurface);
    }
    if (presentBlit) {
#ifdef __APPLE__
      if (device) {
        dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
      }
#endif
      std::unique_lock<std::shared_mutex> lock(_mutex);
      // Present only if the blitted texture is still the surface's current
      // one. The epoch changes on any acquire, present, configure, detach, or
      // adoption, so a frame that started - even one that already completed -
      // or any other transition while we were unlocked skips this present
      // (their newer content stands; presenting here would be a Dawn
      // present-without-acquire error).
      if (_surface && !_frameInFlight && _frameEpoch == blitEpoch) {
        _surface.Present();
      }
    }
  }

  // --- Rendering thread
  // -------------------------------------------------------

  void configure(wgpu::SurfaceConfiguration &newConfig,
                 std::vector<wgpu::TextureFormat> viewFormats) {
    applyPendingAttach(/* supersedeInFlightFrame = */ true);
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _viewFormats = std::move(viewFormats);
    _config = newConfig;
    // The caller's viewFormats storage dies with the call; point the stored
    // configuration at our own copy.
    _config.viewFormats = _viewFormats.empty() ? nullptr : _viewFormats.data();
    _config.viewFormatCount = _viewFormats.size();
    // The drawing buffer starts at the canvas size. Clamp so a canvas that has
    // not been laid out yet (0x0) configures instead of erroring.
    _config.width = std::max(1, _width);
    _config.height = std::max(1, _height);
    _config.presentMode = wgpu::PresentMode::Fifo;
    _texture = nullptr;
    _frameEpoch++;
#if defined(__ANDROID__)
    _hardwareBufferPresenter.configure(_config);
#endif
    _blitPresenter.configure(_config);
    _configureLocked();
  }

  // Resize the drawing buffer (canvas.width/height changed).
  void reconfigure(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      return;
    }
    _config.width = std::max(1, newWidth);
    _config.height = std::max(1, newHeight);
    _texture = nullptr;
    _frameEpoch++;
    _configureLocked();
  }

  void unconfigure() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_surface) {
      _surface.Unconfigure();
    }
    _texture = nullptr;
    _config = {};
    _viewFormats.clear();
    _acquiredFromSurface = false;
    _frameEpoch++;
#if defined(__ANDROID__)
    _hardwareBufferPresenter.unconfigure();
#endif
    _blitPresenter.unconfigure();
  }

  bool isConfigured() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _config.device != nullptr;
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Called right before `device` is destroyed. If the Dawn surface holds a
  // swapchain for that device, drop the surface now, while the device is
  // still alive, and recreate an unconfigured one for the same native window
  // so a later configure() with another device still renders on screen.
  //
  // Why: on Vulkan, tearing a swapchain down (SwapChain::DetachFromSurfaceImpl)
  // goes through the device's FencedDeleter. Dawn only tears it down when the
  // surface is reconfigured or destroyed, and Unconfigure() is not enough (it
  // parks the swapchain as "recycled" for the next Configure). So when the
  // native view is dropped after device.destroy(), ~Surface dereferences the
  // dead device (SIGSEGV, fault addr 0x20). Android only: Metal's detach does
  // not touch the device, and this must not change behaviour there.
  //
  // Remove once the pinned Dawn contains the upstream fix; the check in
  // scripts/install-dawn.ts fails the install when the Dawn pin changes so
  // that decision is not forgotten.
  void releaseSurfaceForDevice(const wgpu::Device &device) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    // The presenters' frames (and the blit presenter's own swapchain) belong
    // to the device too: drop them while it is alive.
    _blitPresenter.releaseForDevice(device);
#if defined(__ANDROID__)
    _hardwareBufferPresenter.releaseForDevice(device);
    if (!_surface || !_surfaceDevice || _surfaceDevice.Get() != device.Get()) {
      return;
    }
    // Left unconfigured: _config still names the destroyed device, and
    // configuring against it would only raise a validation error. The next
    // configure() from JS (or a surface re-attach) configures it.
    recreateSurfaceLocked();
#endif
  }

  // True while a native view owns a surface for this context (attached or
  // pending adoption). Used to decide which side retires the registry entry:
  // the native view's teardown when a surface exists, the JS Canvas cleanup
  // otherwise (see RNWebGPU::destroyContext).
  bool hasNativeSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    // A view presenting through a FramePresenter owns presentation the same
    // way a surface-backed view does, and retires the entry on its own
    // teardown.
    if (activePresenter() != nullptr) {
      return true;
    }
    return _nativeSurface != nullptr || _hasPendingAttach;
  }

  // Returns the texture for the current frame: the surface's swapchain texture
  // when a surface is attached and healthy, an offscreen texture otherwise.
  // Never returns null; throws when called before configure(). When frameEpoch
  // is given, it receives the epoch identifying this frame (see
  // isCurrentFrame).
  wgpu::Texture getCurrentTexture(uint64_t *frameEpoch = nullptr) {
    // Start-of-frame boundary; a new acquire supersedes any previous frame
    // that never presented.
    applyPendingAttach(/* supersedeInFlightFrame = */ true);
    if (FramePresenter *presenter = activePresenter()) {
      // The presenter's frames track the drawing buffer (_config.width/height,
      // kept in sync with canvas.width/height by reconfigure()), like the
      // swapchain, so the canvas texture always matches the app's other
      // attachments.
      int width = 0;
      int height = 0;
      {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        if (_config.device != nullptr) {
          width = static_cast<int>(_config.width);
          height = static_cast<int>(_config.height);
        }
      }
      presenter->resize(width, height);
      if (auto texture = presenter->getCurrentTexture()) {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        if (_adoptOffscreenFrame) {
          // The offscreen frame that was in flight when the blit presenter
          // took over never presented: it is superseded by this one.
          _adoptOffscreenFrame = false;
          _texture = nullptr;
        }
        _frameInFlight = true;
        _acquiredFromSurface = false;
        _frameEpoch++;
        if (frameEpoch) {
          *frameEpoch = _frameEpoch;
        }
        return texture;
      }
      // The presenter cannot take a frame (pool not allocated yet or
      // unsupported, view detached mid-frame, a free-slot timeout): fall
      // through to the offscreen fallback so the render loop survives; this
      // frame is simply not shown.
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      throw std::runtime_error(
          "[WebGPU] getCurrentTexture() called on a canvas context that is "
          "not configured; call context.configure() first");
    }
    _frameInFlight = true;
    _acquiredFromSurface = false;
    _frameEpoch++;
    if (frameEpoch) {
      *frameEpoch = _frameEpoch;
    }
    if (_surface) {
      auto texture = acquireSurfaceTextureLocked();
      if (texture) {
        _acquiredFromSurface = true;
        return texture;
      }
      // The surface is transiently unusable (e.g. mid-resize, lost while
      // backgrounding): fall back to an offscreen texture so the render loop
      // survives; this frame is simply not presented.
    }
    if (!_texture) {
      _texture = createOffscreenTextureLocked();
    }
    return _texture;
  }

  // True while the frame started by the getCurrentTexture() call that
  // returned frameEpoch is still the current one: nothing presented,
  // configured, resized, unconfigured, detached, or adopted a surface since.
  // A pending attach also ends it, so a frame that never presents cannot block
  // adoption: the next getCurrentTexture() supersedes it, as an uncached call
  // would.
  bool isCurrentFrame(uint64_t frameEpoch) {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _frameInFlight && !_hasPendingAttach && _frameEpoch == frameEpoch;
  }

  // Present the current frame. Runs synchronously on the thread that did
  // getCurrentTexture/submit (main JS, Reanimated UI, or a worklet runtime),
  // preserving Dawn surface thread-affinity. Frames whose texture was not
  // acquired from the attached surface (offscreen, detached mid-frame, or
  // acquire failure) are dropped. This is also the end-of-frame boundary: it
  // adopts a surface that attached while the frame was in flight.
  void presentFrame() {
    FramePresenter *presenter = activePresenter();
    if (presenter != nullptr) {
      // Hands the frame to the view, which puts it on screen from the UI
      // thread. A no-op for a frame that was rendered offscreen.
      presenter->present();
    }
    // An offscreen frame the blit presenter is waiting for, see enableBlit().
    wgpu::Texture offscreenFrame;
#ifdef __APPLE__
    // Ensure command buffers are scheduled before presenting. Read the device
    // under a shared lock, then wait without holding it (the wait can block).
    // Not needed when a presenter took the frame: it presents later, from
    // the UI thread, and does its own waiting.
    wgpu::Device device;
    if (presenter == nullptr) {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      device = _config.device;
    }
    if (device) {
      dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
    }
#endif
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_surface && _acquiredFromSurface) {
        _surface.Present();
      }
      _acquiredFromSurface = false;
      _frameInFlight = false;
      _frameEpoch++;
      if (_adoptOffscreenFrame) {
        _adoptOffscreenFrame = false;
        if (_blitPresenter.isEnabled()) {
          offscreenFrame = std::exchange(_texture, nullptr);
        }
      }
    }
    adoptOffscreenFrame(std::move(offscreenFrame));
    applyPendingAttach();
  }

  NativeInfo getNativeInfo() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    // A surface that is still pending adoption is the one callers should see.
    void *native = _hasPendingAttach ? _pendingNativeSurface : _nativeSurface;
    return {.nativeSurface = native, .width = _width, .height = _height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.width = _width, .height = _height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _config;
  }

  // --- Presenter modes (called from the platform view glue) -----------------

#if defined(__ANDROID__)

  // Turn on pool mode for this context. dpW/dpH is the canvas-client (dp) size
  // reported to JS via getSize(); the actual buffers are sized in pool px from
  // the canvas drawing buffer in HardwareBufferPresenter::resize().
  void enablePool(int dpW, int dpH) {
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _width = dpW;
      _height = dpH;
    }
    _hardwareBufferPresenter.enable();
  }

  // Set the canvas-client (dp) size without changing pool mode. Keeps getSize()
  // (and thus the JS canvas.clientWidth/Height) in sync on resize.
  void setPoolClientSize(int dpW, int dpH) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _width = dpW;
    _height = dpH;
  }

  // The pool itself: the view consumes finished frames from it directly.
  HardwareBufferPresenter &hardwareBufferPresenter() {
    return _hardwareBufferPresenter;
  }
#endif

  // Turn on blit mode for this context: frames are rendered into textures the
  // BlitPresenter owns and copied onto the view's surface on the UI thread.
  // dpW/dpH is the canvas-client (dp) size reported to JS via getSize(). The
  // latest frame rendered offscreen before this point is handed over so it
  // shows up without waiting for the next render.
  void enableBlit(int dpW, int dpH,
                  BlitPresenter::NativeSurface nativeSurface) {
    wgpu::Texture offscreenFrame;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _width = dpW;
      _height = dpH;
      _blitPresenter.enable(std::move(nativeSurface));
      if (_frameInFlight) {
        // A frame is being rendered offscreen right now. It keeps its texture
        // (getCurrentTexture() must not change mid-frame) and is handed over
        // once it is finished, in presentFrame().
        _adoptOffscreenFrame = _texture != nullptr;
      } else {
        offscreenFrame = std::exchange(_texture, nullptr);
      }
    }
    adoptOffscreenFrame(std::move(offscreenFrame));
  }

  // The blit presenter itself: the view presents finished frames through it.
  BlitPresenter &blitPresenter() { return _blitPresenter; }

private:
  void detach(bool createFallbackTexture) {
    void *releasedSurfaces[2] = {nullptr, nullptr};
    NativeSurfaceReleaser releasers[2];
    // Destroyed outside the lock: they hold references to the view.
    BlitPresenter::Disabled releasedBlit;
#if defined(__ANDROID__)
    HardwareBufferPresenter::Callbacks releasedPoolCallbacks;
#endif
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
#if defined(__ANDROID__)
      // Leaving pool mode (the WebGPUHardwareBufferView detached): drop the
      // pool. A configured context keeps rendering into the offscreen
      // fallback, exactly like the surface path.
      if (_hardwareBufferPresenter.isEnabled()) {
        releasedPoolCallbacks = _hardwareBufferPresenter.disable();
        if (createFallbackTexture && _config.device != nullptr && !_texture) {
          _texture = createOffscreenTextureLocked();
        }
        _frameEpoch++;
      }
#endif
      // Leaving blit mode (the view's surface went away). The presenter's
      // latest frame becomes the offscreen one, so the next surface or
      // presenter shows it without waiting for a render.
      if (_blitPresenter.isEnabled()) {
        releasedBlit = _blitPresenter.disable();
        _adoptOffscreenFrame = false;
        if (createFallbackTexture && _config.device != nullptr) {
          auto &frame = releasedBlit.lastFrame;
          // Only a frame at the current drawing buffer size can be rendered
          // into again.
          if (frame && frame.GetWidth() == _config.width &&
              frame.GetHeight() == _config.height) {
            _texture = std::move(frame);
          } else if (!_texture) {
            _texture = createOffscreenTextureLocked();
          }
        }
        _frameEpoch++;
      }
      // The platform is tearing surfaces down; a not-yet-adopted attach is
      // stale, cancel it.
      if (_hasPendingAttach) {
        _hasPendingAttach = false;
        _pendingSurface = nullptr;
        releasedSurfaces[0] = _pendingNativeSurface;
        releasers[0] = std::move(_pendingReleaser);
        _pendingNativeSurface = nullptr;
      }
      if (_surface) {
        if (createFallbackTexture && _config.device != nullptr) {
          _texture = createOffscreenTextureLocked();
        }
        _surface = nullptr;
#if defined(__ANDROID__)
        // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
        _surfaceDevice = nullptr;
#endif
        // The in-flight frame (if any) rendered into the destroyed surface;
        // presentFrame() must not present it.
        _acquiredFromSurface = false;
      }
      _frameEpoch++;
      // Window ownership is independent of the Dawn surface handle (which can
      // be null if surface creation failed): always return the window.
      releasedSurfaces[1] = _nativeSurface;
      releasers[1] = std::move(_releaser);
      _nativeSurface = nullptr;
    }
    // Release outside the lock: the platform may do real work here.
    for (int i = 0; i < 2; i++) {
      if (releasers[i] && releasedSurfaces[i]) {
        releasers[i](releasedSurfaces[i]);
      }
    }
  }

  // The presenter frames currently go to, or null in swapchain / offscreen
  // mode. A view enables its own presenter and disables it on the way out, so
  // at most one is enabled at a time.
  FramePresenter *activePresenter() {
#if defined(__ANDROID__)
    if (_hardwareBufferPresenter.isEnabled()) {
      return &_hardwareBufferPresenter;
    }
#endif
    if (_blitPresenter.isEnabled()) {
      return &_blitPresenter;
    }
    return nullptr;
  }

  // Hands a finished offscreen frame to the blit presenter. Must be called
  // without _mutex: adopting wakes the view. If the presenter went away in the
  // meantime, the frame goes back to being the offscreen one.
  void adoptOffscreenFrame(wgpu::Texture frame) {
    if (!frame || _blitPresenter.adoptFrame(frame)) {
      return;
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (!_texture && _config.device != nullptr &&
        frame.GetWidth() == _config.width &&
        frame.GetHeight() == _config.height) {
      _texture = std::move(frame);
    }
  }

  // All *Locked helpers below require _mutex to be held exclusively.

  wgpu::Texture createOffscreenTextureLocked() {
    wgpu::TextureDescriptor descriptor;
    // Union with the user's usage so offscreen frames stay compatible with
    // whatever they configured (e.g. CopySrc readbacks). RenderAttachment |
    // CopySrc | TextureBinding is what the fallback itself needs (rendering,
    // the attach blit, sampling).
    descriptor.usage = _config.usage | wgpu::TextureUsage::RenderAttachment |
                       wgpu::TextureUsage::CopySrc |
                       wgpu::TextureUsage::TextureBinding;
    descriptor.format = _config.format;
    descriptor.size.width = std::max(1u, _config.width);
    descriptor.size.height = std::max(1u, _config.height);
    descriptor.viewFormats = _config.viewFormats;
    descriptor.viewFormatCount = _config.viewFormatCount;
    return _config.device.CreateTexture(&descriptor);
  }

  // Acquire the surface's current texture, reconfiguring once when the surface
  // reports it is stale (rotation, resize, coming back from background).
  wgpu::Texture acquireSurfaceTextureLocked() {
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (!isAcquireSuccess(surfaceTexture)) {
      if (surfaceTexture.status ==
          wgpu::SurfaceGetCurrentTextureStatus::Error) {
        return nullptr;
      }
      configureSurfaceLocked(_config);
      // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE: the reconfigure
      // may have replaced the surface and failed to create a new one.
      if (!_surface) {
        return nullptr;
      }
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

  // Copy the last offscreen frame onto the freshly attached surface. Returns
  // true when the copy was submitted and the surface should be presented.
  bool blitOffscreenToSurfaceLocked() {
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (!isAcquireSuccess(surfaceTexture)) {
      return false;
    }

    wgpu::TexelCopyTextureInfo source = {};
    source.texture = _texture;
    wgpu::TexelCopyTextureInfo destination = {};
    destination.texture = surfaceTexture.texture;

    // The offscreen frame and the new surface can disagree on size (e.g. the
    // device rotated while detached); copy the shared region.
    wgpu::Extent3D size = {
        std::min(_texture.GetWidth(), surfaceTexture.texture.GetWidth()),
        std::min(_texture.GetHeight(), surfaceTexture.texture.GetHeight()), 1};

    wgpu::CommandEncoderDescriptor encoderDescriptor;
    wgpu::CommandEncoder encoder =
        _config.device.CreateCommandEncoder(&encoderDescriptor);
    encoder.CopyTextureToTexture(&source, &destination, &size);
    wgpu::CommandBuffer commands = encoder.Finish();
    _config.device.GetQueue().Submit(1, &commands);
    return true;
  }

  void _configureLocked() {
    if (_surface) {
      configureSurfaceLocked(_config);
#ifdef __APPLE__
      applyCAMetalLayerColorSpace(_nativeSurface, _config.format);
#endif
    } else {
      _texture = createOffscreenTextureLocked();
    }
  }

  // Every Surface::Configure goes through here. Once the
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE lines are gone, this
  // is a plain wrapper around _surface.Configure.
  void configureSurfaceLocked(const wgpu::SurfaceConfiguration &config) {
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    // Vulkan refuses to switch a surface to another device and keeps the old
    // swapchain attached, so a device change (device swap, recovery after a
    // lost device, a re-run effect) is done with a fresh surface. This also
    // tears the old swapchain down while its device is still alive.
    if (_surfaceDevice && _surfaceDevice.Get() != config.device.Get()) {
      recreateSurfaceLocked();
      if (!_surface) {
        return;
      }
    }
#endif
    _surface.Configure(&config);
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = config.device;
#endif
  }

#if defined(__ANDROID__)
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Replace _surface with a new, unconfigured surface for the same native
  // window. Dropping the old one runs ~Surface, which detaches both the
  // active and the recycled swapchain against their still-alive device.
  void recreateSurfaceLocked() {
    _surface = nullptr;
    _surfaceDevice = nullptr;
    // A frame acquired from the old surface can no longer be presented.
    _acquiredFromSurface = false;
    _frameEpoch++;
    if (_nativeSurface) {
      wgpu::SurfaceSourceAndroidNativeWindow source;
      source.window = _nativeSurface;
      wgpu::SurfaceDescriptor descriptor;
      descriptor.nextInChain = &source;
      _surface = _gpu.CreateSurface(&descriptor);
    }
  }
#endif

  mutable std::shared_mutex _mutex;
#if defined(__ANDROID__)
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // The device _surface was last configured with. Dawn keeps a swapchain for
  // that device inside the surface even after Unconfigure() (it is recycled
  // for the next Configure), so this outlives _config.device on purpose and
  // is only reset when _surface itself is dropped or replaced. Owning: Dawn
  // destroys a device when its last external reference goes away, which
  // would leave the swapchain with a dead device just like an explicit
  // destroy(). Declared before _surface so it is released after it.
  //
  // Android only: this is an extra owning reference to the device, and on
  // Metal (where the detach never touches the device) it would keep a
  // device alive past its last JS reference for no reason.
  wgpu::Device _surfaceDevice = nullptr;
#endif
  // Attached on-screen surface (null while offscreen).
  void *_nativeSurface = nullptr;
  wgpu::Surface _surface = nullptr;
  NativeSurfaceReleaser _releaser;
  // Offscreen fallback drawing buffer.
  wgpu::Texture _texture = nullptr;
  // Surface attached by the UI thread, awaiting adoption at a frame boundary.
  bool _hasPendingAttach = false;
  void *_pendingNativeSurface = nullptr;
  wgpu::Surface _pendingSurface = nullptr;
  NativeSurfaceReleaser _pendingReleaser;
  // Frame state: set by getCurrentTexture, cleared by presentFrame.
  bool _frameInFlight = false;
  bool _acquiredFromSurface = false;
  // Bumped on every acquire, present, configure/reconfigure/unconfigure,
  // adoption, and detach. The deferred blit-present in applyPendingAttach
  // revalidates against it so it never presents a texture that stopped being
  // the surface's current one while the lock was released.
  uint64_t _frameEpoch = 0;
  // device == nullptr means "not configured". _viewFormats owns the storage
  // that _config.viewFormats points at.
  wgpu::SurfaceConfiguration _config;
  std::vector<wgpu::TextureFormat> _viewFormats;
  // Keeps the Dawn instance alive for as long as any canvas exists.
  wgpu::Instance _gpu;
  // Native view size in dp (surfaced as clientWidth/clientHeight on the JS
  // canvas).
  int _width;
  int _height;

  // The presentation modes that hand finished frames to the view instead of
  // presenting a swapchain from the rendering thread: the copy onto the view's
  // surface (BlitPresenter) and, on Android, the AHB pool
  // (WebGPUHardwareBufferView). Each has its own mutex, taken after _mutex and
  // never while blocking in getCurrentTexture().
  BlitPresenter _blitPresenter;
#if defined(__ANDROID__)
  HardwareBufferPresenter _hardwareBufferPresenter;
#endif
  // The frame in flight when blit mode was enabled is rendering into _texture;
  // presentFrame() hands it to the blit presenter once it is finished.
  bool _adoptOffscreenFrame = false;
};

class SurfaceRegistry {
public:
  static SurfaceRegistry &getInstance() {
    static SurfaceRegistry instance;
    return instance;
  }

  SurfaceRegistry(const SurfaceRegistry &) = delete;
  SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;

  std::shared_ptr<SurfaceInfo> getSurfaceInfo(int id) {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    return nullptr;
  }

  void removeSurfaceInfo(int id) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.erase(id);
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoOrCreate(int id, wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    return getSurfaceInfoOrCreateLocked(id, gpu, width, height);
  }

  // Find-or-create + attach as one atomic step under the registry lock, so it
  // serializes with removeSurfaceInfoIfDetached: an attach can never land on
  // an entry that a concurrent destroyContext is erasing (it either marks the
  // entry attached before the check, or re-creates the entry after the
  // erase). Lock order is registry -> SurfaceInfo, matching every other path.
  std::shared_ptr<SurfaceInfo> attachSurface(int id, wgpu::Instance gpu,
                                             int width, int height,
                                             void *nativeSurface,
                                             wgpu::Surface surface,
                                             NativeSurfaceReleaser releaser) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto info = getSurfaceInfoOrCreateLocked(id, gpu, width, height);
    info->attachSurface(nativeSurface, std::move(surface), std::move(releaser));
    return info;
  }

  // Erase the entry only if no native surface is attached or pending; the
  // atomic counterpart of attachSurface above (see RNWebGPU::destroyContext
  // for the ownership split this implements).
  void removeSurfaceInfoIfDetached(int id) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it == _registry.end() || it->second->hasNativeSurface()) {
      return;
    }
    _registry.erase(it);
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // See SurfaceInfo::releaseSurfaceForDevice. Lock order is registry ->
  // SurfaceInfo, matching every other path.
  void releaseSurfacesForDevice(const wgpu::Device &device) {
    std::vector<std::shared_ptr<SurfaceInfo>> infos;
    {
      // Walking _allSurfaces, not _registry: a GPUCanvasContext keeps its own
      // shared_ptr, so an entry erased by clear() (dev reload) or by
      // removeSurfaceInfo() can still be alive and still hold a swapchain for
      // this device.
      std::unique_lock<std::shared_mutex> lock(_mutex);
      pruneAllSurfacesLocked();
      infos.reserve(_allSurfaces.size());
      for (auto &weak : _allSurfaces) {
        if (auto info = weak.lock()) {
          infos.push_back(std::move(info));
        }
      }
    }
    for (auto &info : infos) {
      info->releaseSurfaceForDevice(device);
    }
  }

  // Drops all entries. Called when the RN instance tears down (dev reload):
  // JS context ids restart from scratch, so surviving entries would alias new
  // canvases onto dead surfaces.
  void clear() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.clear();
  }

private:
  SurfaceRegistry() = default;

  std::shared_ptr<SurfaceInfo> getSurfaceInfoOrCreateLocked(int id,
                                                            wgpu::Instance gpu,
                                                            int width,
                                                            int height) {
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry[id] = info;
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    pruneAllSurfacesLocked();
    _allSurfaces.push_back(info);
    return info;
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  void pruneAllSurfacesLocked() {
    _allSurfaces.erase(
        std::remove_if(_allSurfaces.begin(), _allSurfaces.end(),
                       [](const std::weak_ptr<SurfaceInfo> &weak) {
                         return weak.expired();
                       }),
        _allSurfaces.end());
  }

  mutable std::shared_mutex _mutex;
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Every live SurfaceInfo, including the ones no longer in _registry.
  // Non-owning; pruned on insert and on every device destroy.
  std::vector<std::weak_ptr<SurfaceInfo>> _allSurfaces;
};

} // namespace rnwgpu
