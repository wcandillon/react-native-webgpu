#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

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

// Creates a Dawn surface for the platform's native surface pointer. Kept for
// as long as the native surface is attached so the Dawn surface can be
// rebuilt after it had to be dropped (see unconfigureIfDevice). May run on
// the UI thread (attach) or the rendering thread (configure).
using SurfaceFactory = std::function<wgpu::Surface(void *)>;

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
    // Drop the Dawn objects before releasing the native surfaces they borrow.
    _surface = nullptr;
    _surfaceDevice = nullptr;
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
  void attachSurface(void *nativeSurface, SurfaceFactory makeSurface,
                     NativeSurfaceReleaser releaser) {
    // Dawn surface creation only wraps the native handle (the swapchain is
    // created at Configure), so it is cheap enough to do eagerly here.
    wgpu::Surface surface = makeSurface ? makeSurface(nativeSurface) : nullptr;
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
      _pendingSurfaceFactory = std::move(makeSurface);
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
      // Dropping the previous Dawn surface (if any) detaches its swapchain;
      // _surfaceDevice kept that device alive for exactly this.
      _surface = std::move(_pendingSurface);
      _surfaceDevice = nullptr;
      _surfaceFactory = std::move(_pendingSurfaceFactory);
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
    // The Dawn surface is dropped by unconfigure() and when its device is
    // destroyed (unconfigureIfDevice) while the native window stays attached;
    // configure() is the standard recovery from both, so rebuild it here
    // rather than rendering offscreen until the next attach.
    if (!_surface) {
      createSurfaceLocked();
    }
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
    // Drop the surface rather than Surface::Unconfigure it: Dawn parks the
    // swapchain in the surface for reuse by the next Configure, and keeping
    // that swapchain would keep its device pinned (through _surfaceDevice)
    // behind an unconfigured canvas for as long as the canvas stays mounted.
    // Releasing everything lets `context.unconfigure(); device = null` free
    // the device as it does for a canvas without a surface; configure()
    // rebuilds the surface from the factory.
    dropSurfaceLocked();
    _texture = nullptr;
    _config = {};
    _viewFormats.clear();
    _frameEpoch++;
  }

  // Called by GPUDevice::destroy() for every canvas before the device is torn
  // down. Workaround for a Dawn Vulkan bug: SwapChainVk::DetachFromSurfaceImpl
  // (SwapChainVk.cpp) dereferences the device's FencedDeleter, which
  // DeviceVk::DestroyImpl has already nulled, so a ~Surface that runs after
  // the device was destroyed (the native view teardown in detach(), or the
  // Canvas unmount one frame later) SIGSEGVs. Metal only destroys the
  // drawable texture and tolerates the order. Invariant this maintains: no
  // Dawn surface carries a swapchain bound to `device` once destroy() runs.
  // Removable once Dawn guards DetachFromSurfaceImpl against a destroyed
  // device.
  //
  // The match is on _surfaceDevice as well as _config.device: _surfaceDevice
  // is what records which device's swapchain the surface holds, while
  // matching _config.device also releases the offscreen drawing buffer of a
  // canvas that has no surface.
  //
  // _config is kept: per spec a canvas whose device was destroyed stays
  // configured and hands out invalid textures, so a render loop that ticks
  // once more after destroy() no-ops instead of throwing "not configured".
  // The native window and its factory stay too, so configure() with a
  // replacement device rebuilds the surface.
  void unconfigureIfDevice(const wgpu::Device &device) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (!isDeviceLocked(_config.device, device) &&
        !isDeviceLocked(_surfaceDevice, device)) {
      return;
    }
    dropSurfaceLocked();
    _texture = nullptr;
    _frameEpoch++;
  }

  bool isConfigured() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _config.device != nullptr;
  }

  // True while a native view owns a surface for this context (attached or
  // pending adoption). Used to decide which side retires the registry entry:
  // the native view's teardown when a surface exists, the JS Canvas cleanup
  // otherwise (see RNWebGPU::destroyContext).
  bool hasNativeSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _nativeSurface != nullptr || _hasPendingAttach;
  }

  // Returns the texture for the current frame: the surface's swapchain texture
  // when a surface is attached and healthy, an offscreen texture otherwise.
  // Never returns null; throws when called before configure().
  wgpu::Texture getCurrentTexture() {
    // Start-of-frame boundary; a new acquire supersedes any previous frame
    // that never presented.
    applyPendingAttach(/* supersedeInFlightFrame = */ true);
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      throw std::runtime_error(
          "[WebGPU] getCurrentTexture() called on a canvas context that is "
          "not configured; call context.configure() first");
    }
    _frameInFlight = true;
    _acquiredFromSurface = false;
    _frameEpoch++;
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

  // Present the current frame. Runs synchronously on the thread that did
  // getCurrentTexture/submit (main JS, Reanimated UI, or a worklet runtime),
  // preserving Dawn surface thread-affinity. Frames whose texture was not
  // acquired from the attached surface (offscreen, detached mid-frame, or
  // acquire failure) are dropped. This is also the end-of-frame boundary: it
  // adopts a surface that attached while the frame was in flight.
  void presentFrame() {
#ifdef __APPLE__
    // Ensure command buffers are scheduled before presenting. Read the device
    // under a shared lock, then wait without holding it (the wait can block).
    wgpu::Device device;
    {
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
    }
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

private:
  void detach(bool createFallbackTexture) {
    void *releasedSurfaces[2] = {nullptr, nullptr};
    NativeSurfaceReleaser releasers[2];
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      // The platform is tearing surfaces down; a not-yet-adopted attach is
      // stale, cancel it.
      if (_hasPendingAttach) {
        _hasPendingAttach = false;
        _pendingSurface = nullptr;
        releasedSurfaces[0] = _pendingNativeSurface;
        releasers[0] = std::move(_pendingReleaser);
        _pendingNativeSurface = nullptr;
      }
      if (_surface && createFallbackTexture && _config.device != nullptr) {
        _texture = createOffscreenTextureLocked();
      }
      dropSurfaceLocked();
      _frameEpoch++;
      // Window ownership is independent of the Dawn surface handle (which can
      // be null if surface creation failed): always return the window.
      releasedSurfaces[1] = _nativeSurface;
      releasers[1] = std::move(_releaser);
      _surfaceFactory = nullptr;
      _nativeSurface = nullptr;
    }
    // Release outside the lock: the platform may do real work here.
    for (int i = 0; i < 2; i++) {
      if (releasers[i] && releasedSurfaces[i]) {
        releasers[i](releasedSurfaces[i]);
      }
    }
  }

  // All *Locked helpers below require _mutex to be held exclusively.

  static bool isDeviceLocked(const wgpu::Device &held,
                             const wgpu::Device &device) {
    return held != nullptr && held.Get() == device.Get();
  }

  // Release the Dawn surface. ~Surface detaches its swapchain (live or
  // recycled), which needs that swapchain's device alive: _surfaceDevice
  // guarantees it and is released only afterwards.
  void dropSurfaceLocked() {
    _surface = nullptr;
    _surfaceDevice = nullptr;
    // The in-flight frame (if any) rendered into the dropped surface;
    // presentFrame() must not present it.
    _acquiredFromSurface = false;
  }

  // Create the Dawn surface for the attached native window. Requires _surface
  // (and so _surfaceDevice) to be null. False when no window is attached or
  // Dawn surface creation failed; the context then renders offscreen.
  bool createSurfaceLocked() {
    if (_nativeSurface && _surfaceFactory) {
      _surface = _surfaceFactory(_nativeSurface);
    }
    return _surface != nullptr;
  }

  // Every Surface::Configure goes through here so _surfaceDevice tracks the
  // device whose swapchain the surface holds (see unconfigureIfDevice).
  //
  // A surface only ever carries one device's swapchain: when the device
  // changes, the surface is rebuilt rather than reconfigured in place.
  // Surface::Configure would detach the previous swapchain itself, but only
  // after validating the new configuration, so a rejected one would leave the
  // old device's swapchain live behind a surface now attributed to the new
  // device (and invisible to unconfigureIfDevice). The detach also needs the
  // old device alive: dropSurfaceLocked runs it before releasing
  // _surfaceDevice, whereas reassigning _surfaceDevice first would let a
  // device JS no longer references be destroyed mid-detach.
  //
  // Returns false when the surface had to be rebuilt and creation failed
  // (_surface is then null and the caller falls back to offscreen).
  bool configureSurfaceLocked(const wgpu::SurfaceConfiguration &config) {
    if (_surfaceDevice != nullptr &&
        !isDeviceLocked(_surfaceDevice, config.device)) {
      dropSurfaceLocked();
      if (!createSurfaceLocked()) {
        return false;
      }
    }
    _surfaceDevice = config.device;
    _surface.Configure(&config);
#ifdef __APPLE__
    applyCAMetalLayerColorSpace(_nativeSurface, config.format);
#endif
    return true;
  }

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
    if (_surface && configureSurfaceLocked(_config)) {
      return;
    }
    _texture = createOffscreenTextureLocked();
  }

  mutable std::shared_mutex _mutex;
  // Attached on-screen surface (null while offscreen).
  void *_nativeSurface = nullptr;
  wgpu::Surface _surface = nullptr;
  // The device passed to the most recent Configure of _surface: the one its
  // swapchain (live, or recycled by Unconfigure) is bound to. Holding the ref
  // also keeps that device from being auto-destroyed when JS drops its last
  // reference while the swapchain still exists. Null when _surface is null.
  wgpu::Device _surfaceDevice = nullptr;
  SurfaceFactory _surfaceFactory;
  NativeSurfaceReleaser _releaser;
  // Offscreen fallback drawing buffer.
  wgpu::Texture _texture = nullptr;
  // Surface attached by the UI thread, awaiting adoption at a frame boundary.
  bool _hasPendingAttach = false;
  void *_pendingNativeSurface = nullptr;
  wgpu::Surface _pendingSurface = nullptr;
  SurfaceFactory _pendingSurfaceFactory;
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
                                             SurfaceFactory makeSurface,
                                             NativeSurfaceReleaser releaser) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto info = getSurfaceInfoOrCreateLocked(id, gpu, width, height);
    info->attachSurface(nativeSurface, std::move(makeSurface),
                        std::move(releaser));
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

  // Drops all entries. Called when the RN instance tears down (dev reload):
  // JS context ids restart from scratch, so surviving entries would alias new
  // canvases onto dead surfaces.
  void clear() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.clear();
  }

  // Drop every Dawn surface holding a swapchain bound to `device` before it
  // is destroyed; see SurfaceInfo::unconfigureIfDevice for why. Entries are
  // snapshotted under the registry lock and visited outside it: a
  // SurfaceInfo lock can be held across a frame acquire/present on its
  // rendering thread, and waiting on that must not stall the UI thread's
  // attach/remove paths. Lock order stays registry -> SurfaceInfo.
  void unconfigureDevice(const wgpu::Device &device) {
    std::vector<std::shared_ptr<SurfaceInfo>> infos;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      infos.reserve(_registry.size());
      for (auto &entry : _registry) {
        infos.push_back(entry.second);
      }
    }
    for (auto &info : infos) {
      info->unconfigureIfDevice(device);
    }
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
    return info;
  }

  mutable std::shared_mutex _mutex;
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
};

} // namespace rnwgpu
