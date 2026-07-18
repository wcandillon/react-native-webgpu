#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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

class SurfaceInfo {
public:
  SurfaceInfo(wgpu::Instance gpu, int width, int height)
      : gpu(std::move(gpu)), width(width), height(height) {}

  ~SurfaceInfo() { close(); }

  /** Permanently close this session surface so it cannot be mutated again. */
  void close() noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closeCleanupComplete) {
        return;
      }
      _closed = true;
      try {
        _unconfigure();
      } catch (...) {
        // _unconfigure releases every owned resource before rethrowing.
      }
      _closeCleanupComplete = true;
    } catch (...) {
      // Destruction can run during native view/runtime teardown and must not
      // terminate the process if a platform cleanup primitive fails.
    }
  }

  /** Mark terminal under the registry lock; resource cleanup happens later. */
  void markClosed() noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _closed = true;
      _ownerId = kInvalidSurfaceOwnerId;
    } catch (...) {
      // A teardown marker must never escape registry invalidation.
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
    if (_closed) {
      return false;
    }
    if (_ownerId != kInvalidSurfaceOwnerId && ownerId < _ownerId) {
      return false;
    }
    _ownerId = ownerId;
    return true;
  }

  bool isOwnedBy(SurfaceOwnerId ownerId) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return !_closed && ownerId != kInvalidSurfaceOwnerId && _ownerId == ownerId;
  }

  void reconfigure(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _throwIfClosed();
    config.width = newWidth;
    config.height = newHeight;
    _configure();
  }

  void configure(wgpu::SurfaceConfiguration &newConfig) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _throwIfClosed();
    config = newConfig;
    config.width = width;
    config.height = height;
    config.presentMode = wgpu::PresentMode::Fifo;
    _configure();
  }

  void unconfigure() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_closed) {
      return;
    }
    _unconfigure();
  }

private:
  void _unconfigure() {
    _ownerId = kInvalidSurfaceOwnerId;
    std::exception_ptr unconfigureError;
    if (surface && _surfaceConfigured) {
      try {
        surface.Unconfigure();
      } catch (...) {
        unconfigureError = std::current_exception();
      }
    }
    _surfaceConfigured = false;
    texture = nullptr;
    surface = nullptr;
    _releaseNativeSurface();
    config = {};
    if (unconfigureError) {
      std::rethrow_exception(unconfigureError);
    }
  }

public:
  bool switchToOffscreenIfOwnedBy(SurfaceOwnerId ownerId) noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return false;
      }
      try {
        _switchToOffscreen();
      } catch (...) {
        // If Dawn rejects the transition, still release the platform surface
        // and leave this owner with a recoverable, unconfigured entry.
        try {
          _unconfigure();
        } catch (...) {
          // _unconfigure clears every resource before rethrowing.
        }
        _ownerId = ownerId;
        return false;
      }
      return true;
    } catch (...) {
      return false;
    }
  }

  bool unconfigureIfOwnedBy(SurfaceOwnerId ownerId) noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return false;
      }
      try {
        _unconfigure();
      } catch (...) {
        // _unconfigure already released the resources before rethrowing.
      }
      // This is a recoverable attach failure, not a view detach. Keep the
      // owner so the next surface callback can retry and later teardown can
      // still remove the exact registry entry.
      _ownerId = ownerId;
      return true;
    } catch (...) {
      return false;
    }
  }

  bool
  switchToOnscreenIfOwnedBy(SurfaceOwnerId ownerId, void *newNativeSurface,
                            wgpu::Surface newSurface,
                            std::shared_ptr<void> nativeSurfaceOwner = {}) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
      return false;
    }
    _switchToOnscreen(newNativeSurface, std::move(newSurface),
                      std::move(nativeSurfaceOwner));
    return true;
  }

  bool resizeIfOwnedBy(SurfaceOwnerId ownerId, int newWidth,
                       int newHeight) noexcept {
    try {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_closed || ownerId == kInvalidSurfaceOwnerId || _ownerId != ownerId) {
        return false;
      }
      width = newWidth;
      height = newHeight;
      return true;
    } catch (...) {
      return false;
    }
  }

private:
  void _switchToOffscreen() {
    // We only do this if the onscreen surface is configured.
    const bool isConfigured = config.device != nullptr;
    if (surface && _surfaceConfigured) {
      surface.Unconfigure();
    }
    _surfaceConfigured = false;
    if (isConfigured) {
      wgpu::TextureDescriptor textureDesc;
      textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                          wgpu::TextureUsage::CopySrc |
                          wgpu::TextureUsage::TextureBinding;
      textureDesc.format = config.format;
      textureDesc.size.width = config.width;
      textureDesc.size.height = config.height;
      texture = config.device.CreateTexture(&textureDesc);
    }
    surface = nullptr;
    _releaseNativeSurface();
  }

  void _switchToOnscreen(void *newNativeSurface, wgpu::Surface newSurface,
                         std::shared_ptr<void> nativeSurfaceOwner) {
    if (surface && _surfaceConfigured) {
      surface.Unconfigure();
    }
    _surfaceConfigured = false;
    surface = nullptr;
    _releaseNativeSurface();
    nativeSurface = newNativeSurface;
    _nativeSurfaceOwner = std::move(nativeSurfaceOwner);
    surface = std::move(newSurface);
    // If we are comming from an offscreen context, we need to configure the new
    // surface
    if (texture != nullptr) {
      config.usage = config.usage | wgpu::TextureUsage::CopyDst;
      _configure();
      // We flush the offscreen texture to the onscreen one
      // TODO: there is a faster way to do this without validation?
      wgpu::CommandEncoderDescriptor encoderDesc;
      auto device = config.device;
      wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&encoderDesc);

      wgpu::TexelCopyTextureInfo sourceTexture = {};
      sourceTexture.texture = texture;

      wgpu::TexelCopyTextureInfo destinationTexture = {};
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      destinationTexture.texture = surfaceTexture.texture;

      wgpu::Extent3D size = {sourceTexture.texture.GetWidth(),
                             sourceTexture.texture.GetHeight(),
                             sourceTexture.texture.GetDepthOrArrayLayers()};

      encoder.CopyTextureToTexture(&sourceTexture, &destinationTexture, &size);

      wgpu::CommandBuffer commands = encoder.Finish();
      wgpu::Queue queue = device.GetQueue();
      queue.Submit(1, &commands);
      surface.Present();
      texture = nullptr;
    } else if (config.device != nullptr) {
      _configure();
    }
  }

public:
  // Present the current surface texture. Called synchronously from the thread
  // that did getCurrentTexture / submit (via GPUCanvasContext::present), so it
  // preserves Dawn surface thread-affinity. No-op when offscreen / unconfigured
  // (no surface).
  void presentFrame() {
#ifdef __APPLE__
    // Ensure command buffers are scheduled before presenting. Read the device
    // under a shared lock, then wait without holding it (the wait can block).
    // The device may be reconfigured between the two locks; that is safe
    // because present() is called on the rendering thread right after submit(),
    // the wait just flushes that thread's already-submitted work, and the
    // Present() below re-checks `surface` under the unique lock before touching
    // it.
    wgpu::Device device;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      if (_closed) {
        return;
      }
      device = config.device;
    }
    if (device) {
      dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
    }
#endif
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (!_closed && surface) {
      surface.Present();
    }
  }

  wgpu::Texture getCurrentTexture() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (_closed) {
      return nullptr;
    }
    if (surface) {
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      return surfaceTexture.texture;
    } else {
      return texture;
    }
  }

  // True when an on-screen wgpu::Surface is attached (vs offscreen texture).
  bool hasSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return !_closed && surface != nullptr;
  }

  NativeInfo getNativeInfo() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (_closed) {
      return {.nativeSurface = nullptr,
              .nativeSurfaceOwner = {},
              .width = 0,
              .height = 0};
    }
    return {.nativeSurface = nativeSurface,
            .nativeSurfaceOwner = _nativeSurfaceOwner,
            .width = width,
            .height = height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (_closed) {
      return {.width = 0, .height = 0};
    }
    return {.width = width, .height = height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (_closed) {
      return {};
    }
    return config;
  }

  wgpu::Device getDevice() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _closed ? nullptr : config.device;
  }

private:
  void _throwIfClosed() const {
    if (_closed) {
      throw std::runtime_error("WebGPU surface session is no longer active");
    }
  }

  void _releaseNativeSurface() noexcept {
    nativeSurface = nullptr;
    _nativeSurfaceOwner.reset();
  }

  void _configure() {
    if (surface) {
      surface.Configure(&config);
      _surfaceConfigured = true;
#ifdef __APPLE__
      applyCAMetalLayerColorSpace(nativeSurface, config.format);
#endif
    } else {
      _surfaceConfigured = false;
      wgpu::TextureDescriptor textureDesc;
      textureDesc.format = config.format;
      textureDesc.size.width = config.width;
      textureDesc.size.height = config.height;
      textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                          wgpu::TextureUsage::CopySrc |
                          wgpu::TextureUsage::TextureBinding;
      texture = config.device.CreateTexture(&textureDesc);
    }
  }

  mutable std::shared_mutex _mutex;
  void *nativeSurface = nullptr;
  std::shared_ptr<void> _nativeSurfaceOwner;
  wgpu::Surface surface = nullptr;
  wgpu::Texture texture = nullptr;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;
  bool _surfaceConfigured = false;
  bool _closed = false;
  bool _closeCleanupComplete = false;
  SurfaceOwnerId _ownerId{kInvalidSurfaceOwnerId};
  int width;
  int height;
};

class SurfaceRegistry {
public:
  static SurfaceRegistry &getInstance() {
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

    // Extract one entry at a time without allocating a temporary container.
    // Dawn/native cleanup can call platform code, so it always runs after
    // releasing the process-wide registry lock.
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
    if (it != _registry.end()) {
      return it->second;
    }
    return nullptr;
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

    // The terminal marker above prevents resurrection while Dawn/platform
    // cleanup executes without the process-wide registry lock.
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
    _registry[key] = info;
    return info;
  }

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
    _registry[key] = info;
    return info;
  }

private:
  SurfaceRegistry() = default;
  mutable std::shared_mutex _mutex;
  std::unordered_map<SurfaceKey, std::shared_ptr<SurfaceInfo>, SurfaceKeyHash>
      _registry;
  std::unordered_set<RNWebGPUSessionId> _openSessions;
};

} // namespace rnwgpu
