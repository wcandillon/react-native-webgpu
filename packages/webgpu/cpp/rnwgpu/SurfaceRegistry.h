#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "RNWebGPUSession.h"
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
  std::shared_ptr<void> nativeSurfaceOwner;
  int width;
  int height;
};

struct Size {
  int width;
  int height;
};

using SurfaceOwnerId = std::uint64_t;
inline constexpr SurfaceOwnerId kInvalidSurfaceOwnerId = 0;

struct SurfaceKey {
  RNWebGPUSessionId sessionId;
  int contextId;

  bool operator==(const SurfaceKey &) const noexcept = default;
};

struct SurfaceKeyHash {
  std::size_t operator()(const SurfaceKey &key) const noexcept {
    const auto sessionHash = std::hash<RNWebGPUSessionId>{}(key.sessionId);
    const auto contextHash = std::hash<int>{}(key.contextId);
    return sessionHash ^ (contextHash + 0x9e3779b9U + (sessionHash << 6U) +
                          (sessionHash >> 2U));
  }
};

// Bridges the asynchronous native surface lifecycle (surfaces appear and
// disappear on the platform UI thread) with the synchronous WebGPU canvas API
// (the JS render loop must always be able to acquire a texture).
//
// Ownership & threading model:
// - A registry entry is keyed by runtime session + contextId. A native view
//   claims it with a monotonically increasing ownerId, so callbacks from a
//   recycled/stale view cannot detach a newer view's surface. Session teardown
//   closes all of that runtime's entries without touching a replacement
//   runtime whose contextId counter restarted from the same value.
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

  ~SurfaceInfo() { close(); }

  void close() noexcept {
    std::shared_ptr<void> nativeSurfaceOwner;
    std::shared_ptr<void> pendingNativeSurfaceOwner;
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closeCleanupComplete) {
        return;
      }
      _closed = true;
      _ownerId = kInvalidSurfaceOwnerId;
      _surface = nullptr;
      _pendingSurface = nullptr;
      _texture = nullptr;
      _hasPendingAttach = false;
      _nativeSurface = nullptr;
      _pendingNativeSurface = nullptr;
      nativeSurfaceOwner = std::move(_nativeSurfaceOwner);
      pendingNativeSurfaceOwner = std::move(_pendingNativeSurfaceOwner);
      _config = {};
      _viewFormats.clear();
      _frameInFlight = false;
      _acquiredFromSurface = false;
      _closeCleanupComplete = true;
    } catch (...) {
      // Native teardown must never terminate the process.
    }
  }

  void markClosed() noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _closed = true;
      _ownerId = kInvalidSurfaceOwnerId;
    } catch (...) {
      // A terminal marker must never escape registry invalidation.
    }
  }

  bool markClosedIfOwnedBy(SurfaceOwnerId ownerId) noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return false;
      }
      _closed = true;
      _ownerId = kInvalidSurfaceOwnerId;
      return true;
    } catch (...) {
      return false;
    }
  }

  bool claimOwner(SurfaceOwnerId ownerId) {
    if (ownerId == kInvalidSurfaceOwnerId) {
      return false;
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_closed || (_ownerId != kInvalidSurfaceOwnerId && ownerId < _ownerId)) {
      return false;
    }
    _ownerId = ownerId;
    return true;
  }

  bool isOwnedBy(SurfaceOwnerId ownerId) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return !_closed && ownerId != kInvalidSurfaceOwnerId && _ownerId == ownerId;
  }

  // --- Platform UI thread ---------------------------------------------------

  // Store a newly created on-screen surface. It becomes active at the next
  // frame boundary (applyPendingAttach); callers should follow up with
  // RNWebGPUManager::flushPendingSurfaceTransition so contexts that are not
  // currently rendering also pick it up.
  bool attachSurfaceIfOwnedBy(SurfaceOwnerId ownerId, void *nativeSurface,
                              wgpu::Surface surface,
                              std::shared_ptr<void> nativeSurfaceOwner = {}) {
    std::shared_ptr<void> replacedNativeSurfaceOwner;
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
      return false;
    }
    if (_hasPendingAttach) {
      replacedNativeSurfaceOwner = std::move(_pendingNativeSurfaceOwner);
    }
    _hasPendingAttach = true;
    _pendingNativeSurface = nativeSurface;
    _pendingSurface = std::move(surface);
    _pendingNativeSurfaceOwner = std::move(nativeSurfaceOwner);
    return true;
  }

  // The platform surface is being destroyed: detach immediately. If the
  // context is configured, rendering continues into an offscreen texture whose
  // content is blitted to the next attached surface; present() no-ops until
  // then. Safe to call when already offscreen.
  bool switchToOffscreenIfOwnedBy(SurfaceOwnerId ownerId) noexcept {
    try {
      if (!isOwnedBy(ownerId)) {
        return false;
      }
      detach(/* createFallbackTexture = */ true, ownerId);
      return true;
    } catch (...) {
      return false;
    }
  }

  // Detach without creating the offscreen fallback: used when the context is
  // being destroyed and nothing will consume further frames.
  void detachSurfaceIfOwnedBy(SurfaceOwnerId ownerId) noexcept {
    try {
      detach(/* createFallbackTexture = */ false, ownerId);
    } catch (...) {
      // View teardown must not escape into Java/Objective-C deallocation.
    }
  }

  // Reflects native view layout changes. Does not resize the drawing buffer:
  // that tracks canvas.width/height (like on the web), see
  // GPUCanvasContext::getCurrentTexture.
  bool resizeIfOwnedBy(SurfaceOwnerId ownerId, int newWidth,
                       int newHeight) noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return false;
      }
      _width = newWidth;
      _height = newHeight;
      return true;
    } catch (...) {
      return false;
    }
  }

  void resize(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    throwIfClosedLocked();
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
    std::uint64_t blitEpoch = 0;
    wgpu::Device device = nullptr;
    std::shared_ptr<void> replacedNativeSurfaceOwner;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed) {
        return;
      }
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
      replacedNativeSurfaceOwner = std::move(_nativeSurfaceOwner);
      _surface = std::move(_pendingSurface);
      _nativeSurface = _pendingNativeSurface;
      _nativeSurfaceOwner = std::move(_pendingNativeSurfaceOwner);
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
        _surface.Configure(&config);
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
      if (!_closed && _surface && !_frameInFlight && _frameEpoch == blitEpoch) {
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
    throwIfClosedLocked();
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
    _configureLocked();
  }

  // Resize the drawing buffer (canvas.width/height changed).
  void reconfigure(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    throwIfClosedLocked();
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
    if (_closed) {
      return;
    }
    if (_surface) {
      _surface.Unconfigure();
    }
    _texture = nullptr;
    _config = {};
    _viewFormats.clear();
    _acquiredFromSurface = false;
    _frameEpoch++;
  }

  bool unconfigureIfOwnedBy(SurfaceOwnerId ownerId) noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return false;
      }
      if (_surface) {
        _surface.Unconfigure();
      }
      _texture = nullptr;
      _config = {};
      _viewFormats.clear();
      _acquiredFromSurface = false;
      _frameEpoch++;
      return true;
    } catch (...) {
      return false;
    }
  }

  bool isConfigured() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return !_closed && _config.device != nullptr;
  }

  // True while a native view owns a surface for this context (attached or
  // pending adoption). Used to decide which side retires the registry entry:
  // the native view's teardown when a surface exists, the JS Canvas cleanup
  // otherwise (see RNWebGPU::destroyContext).
  bool hasNativeSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return !_closed && _ownerId != kInvalidSurfaceOwnerId;
  }

  // Returns the texture for the current frame: the surface's swapchain texture
  // when a surface is attached and healthy, an offscreen texture otherwise.
  // Never returns null; throws when called before configure().
  wgpu::Texture getCurrentTexture() {
    // Start-of-frame boundary; a new acquire supersedes any previous frame
    // that never presented.
    applyPendingAttach(/* supersedeInFlightFrame = */ true);
    std::unique_lock<std::shared_mutex> lock(_mutex);
    throwIfClosedLocked();
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
      if (_closed) {
        return;
      }
      device = _config.device;
    }
    if (device) {
      dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
    }
#endif
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed) {
        return;
      }
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
    if (_closed) {
      return {.nativeSurface = nullptr,
              .nativeSurfaceOwner = {},
              .width = 0,
              .height = 0};
    }
    // A surface that is still pending adoption is the one callers should see.
    void *native = _hasPendingAttach ? _pendingNativeSurface : _nativeSurface;
    auto nativeSurfaceOwner =
        _hasPendingAttach ? _pendingNativeSurfaceOwner : _nativeSurfaceOwner;
    return {.nativeSurface = native,
            .nativeSurfaceOwner = std::move(nativeSurfaceOwner),
            .width = _width,
            .height = _height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (_closed) {
      return {.width = 0, .height = 0};
    }
    return {.width = _width, .height = _height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _closed ? wgpu::SurfaceConfiguration{} : _config;
  }

private:
  void detach(bool createFallbackTexture, SurfaceOwnerId ownerId) {
    std::shared_ptr<void> nativeSurfaceOwner;
    std::shared_ptr<void> pendingNativeSurfaceOwner;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return;
      }
      // The platform is tearing surfaces down; a not-yet-adopted attach is
      // stale, cancel it.
      if (_hasPendingAttach) {
        _hasPendingAttach = false;
        _pendingSurface = nullptr;
        _pendingNativeSurface = nullptr;
        pendingNativeSurfaceOwner = std::move(_pendingNativeSurfaceOwner);
      }
      if (_surface) {
        if (createFallbackTexture && _config.device != nullptr) {
          _texture = createOffscreenTextureLocked();
        }
        _surface = nullptr;
        // The in-flight frame (if any) rendered into the destroyed surface;
        // presentFrame() must not present it.
        _acquiredFromSurface = false;
      }
      _frameEpoch++;
      _nativeSurface = nullptr;
      nativeSurfaceOwner = std::move(_nativeSurfaceOwner);
    }
  }

  // All *Locked helpers below require _mutex to be held exclusively.

  void throwIfClosedLocked() const {
    if (_closed) {
      throw std::runtime_error("WebGPU surface session is no longer active");
    }
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
      _surface.Configure(&_config);
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
      _surface.Configure(&_config);
#ifdef __APPLE__
      applyCAMetalLayerColorSpace(_nativeSurface, _config.format);
#endif
    } else {
      _texture = createOffscreenTextureLocked();
    }
  }

  mutable std::shared_mutex _mutex;
  // Attached on-screen surface (null while offscreen).
  void *_nativeSurface = nullptr;
  std::shared_ptr<void> _nativeSurfaceOwner;
  wgpu::Surface _surface = nullptr;
  // Offscreen fallback drawing buffer.
  wgpu::Texture _texture = nullptr;
  // Surface attached by the UI thread, awaiting adoption at a frame boundary.
  bool _hasPendingAttach = false;
  void *_pendingNativeSurface = nullptr;
  std::shared_ptr<void> _pendingNativeSurfaceOwner;
  wgpu::Surface _pendingSurface = nullptr;
  // Frame state: set by getCurrentTexture, cleared by presentFrame.
  bool _frameInFlight = false;
  bool _acquiredFromSurface = false;
  // Bumped on every acquire, present, configure/reconfigure/unconfigure,
  // adoption, and detach. The deferred blit-present in applyPendingAttach
  // revalidates against it so it never presents a texture that stopped being
  // the surface's current one while the lock was released.
  std::uint64_t _frameEpoch = 0;
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
  bool _closed = false;
  bool _closeCleanupComplete = false;
  SurfaceOwnerId _ownerId{kInvalidSurfaceOwnerId};
};

class SurfaceRegistry {
public:
  static SurfaceRegistry &getInstance() {
    // Native views can be released during process-wide static teardown.
    static auto *instance = new SurfaceRegistry();
    return *instance;
  }

  SurfaceRegistry(const SurfaceRegistry &) = delete;
  SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;

  void openSession(RNWebGPUSessionId sessionId) {
    if (sessionId == kInvalidRNWebGPUSessionId) {
      return;
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _openSessions.insert(sessionId);
  }

  void closeSession(RNWebGPUSessionId sessionId) noexcept {
    if (sessionId == kInvalidRNWebGPUSessionId) {
      return;
    }

    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _openSessions.erase(sessionId);
    }

    // Release Dawn/platform resources without holding the registry lock.
    for (;;) {
      std::shared_ptr<SurfaceInfo> staleSurface;
      {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        auto stale = _registry.end();
        for (auto it = _registry.begin(); it != _registry.end(); ++it) {
          if (it->first.sessionId == sessionId) {
            stale = it;
            break;
          }
        }
        if (stale == _registry.end()) {
          return;
        }
        if (stale->second) {
          stale->second->markClosed();
          staleSurface = std::move(stale->second);
        }
        _registry.erase(stale);
      }
      if (staleSurface) {
        staleSurface->close();
      }
    }
  }

  std::shared_ptr<SurfaceInfo> getSurfaceInfo(RNWebGPUSessionId sessionId,
                                              int id) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    const auto it = _registry.find(SurfaceKey{sessionId, id});
    return it == _registry.end() ? nullptr : it->second;
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoIfOwnedBy(RNWebGPUSessionId sessionId, int id,
                          SurfaceOwnerId ownerId) const {
    auto surfaceInfo = getSurfaceInfo(sessionId, id);
    return surfaceInfo && surfaceInfo->isOwnedBy(ownerId) ? surfaceInfo
                                                          : nullptr;
  }

  bool removeSurfaceInfoIfOwnedBy(
      RNWebGPUSessionId sessionId, int id, SurfaceOwnerId ownerId,
      const std::shared_ptr<SurfaceInfo> &expectedSurface) {
    if (ownerId == kInvalidSurfaceOwnerId || !expectedSurface) {
      return false;
    }

    std::shared_ptr<SurfaceInfo> removedSurface;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      const auto it = _registry.find(SurfaceKey{sessionId, id});
      if (it == _registry.end() || it->second != expectedSurface ||
          !it->second->markClosedIfOwnedBy(ownerId)) {
        return false;
      }
      removedSurface = std::move(it->second);
      _registry.erase(it);
    }
    removedSurface->close();
    return true;
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoOrCreate(RNWebGPUSessionId sessionId, int id,
                         wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_openSessions.find(sessionId) == _openSessions.end()) {
      return nullptr;
    }
    const SurfaceKey key{sessionId, id};
    const auto it = _registry.find(key);
    if (it != _registry.end()) {
      return it->second;
    }
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry.emplace(key, info);
    return info;
  }

  // Claiming and find-or-create happen under one registry lock. Because a
  // claimed entry counts as native-owned, destroyContext cannot erase it in
  // the interval between the claim and the latched attach.
  std::shared_ptr<SurfaceInfo> claimSurfaceInfo(RNWebGPUSessionId sessionId,
                                                int id, SurfaceOwnerId ownerId,
                                                wgpu::Instance gpu, int width,
                                                int height) {
    if (ownerId == kInvalidSurfaceOwnerId) {
      return nullptr;
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_openSessions.find(sessionId) == _openSessions.end()) {
      return nullptr;
    }
    const SurfaceKey key{sessionId, id};
    const auto it = _registry.find(key);
    if (it != _registry.end()) {
      return it->second && it->second->claimOwner(ownerId) ? it->second
                                                           : nullptr;
    }
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    if (!info->claimOwner(ownerId)) {
      return nullptr;
    }
    _registry.emplace(key, info);
    return info;
  }

  void removeSurfaceInfoIfDetached(RNWebGPUSessionId sessionId, int id) {
    std::shared_ptr<SurfaceInfo> removedSurface;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      const auto it = _registry.find(SurfaceKey{sessionId, id});
      if (it == _registry.end() || it->second->hasNativeSurface()) {
        return;
      }
      it->second->markClosed();
      removedSurface = std::move(it->second);
      _registry.erase(it);
    }
    if (removedSurface) {
      removedSurface->close();
    }
  }

private:
  SurfaceRegistry() = default;
  mutable std::shared_mutex _mutex;
  std::unordered_map<SurfaceKey, std::shared_ptr<SurfaceInfo>, SurfaceKeyHash>
      _registry;
  std::unordered_set<RNWebGPUSessionId> _openSessions;
};

} // namespace rnwgpu
