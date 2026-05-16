#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#ifdef __APPLE__
namespace dawn::native::metal {

void WaitForCommandsToBeScheduled(WGPUDevice device);

}
#endif

namespace rnwgpu {

struct NativeInfo {
  void *nativeSurface;
  int width;
  int height;
};

struct Size {
  int width;
  int height;
};

class SurfaceInfo {
public:
  SurfaceInfo(wgpu::Instance gpu, int width, int height)
      : gpu(std::move(gpu)), width(width), height(height) {}

  ~SurfaceInfo() { surface = nullptr; }

  void reconfigure(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    config.width = newWidth;
    config.height = newHeight;
    _configure();
  }

  void configure(wgpu::SurfaceConfiguration &newConfig) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    config = newConfig;
    config.width = width;
    config.height = height;
    config.presentMode = wgpu::PresentMode::Fifo;
    _configure();
  }

  void unconfigure() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _resetPresentationStateLocked();
    if (surface) {
      surface.Unconfigure();
    } else {
      texture = nullptr;
    }
  }

  void *switchToOffscreen() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _resetPresentationStateLocked();
    // We only do this if the onscreen surface is configured.
    auto isConfigured = config.device != nullptr;
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
    return nativeSurface;
  }

  void switchToOnscreen(void *newNativeSurface, wgpu::Surface newSurface) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _resetPresentationStateLocked();
    nativeSurface = newNativeSurface;
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
      _resetPresentationStateLocked();
      texture = nullptr;
    }
  }

  void resize(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    width = newWidth;
    height = newHeight;
  }

  void present() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _presentLocked();
  }

  // Called by the display-link tick. Presents only after the app has submitted
  // work for an acquired texture and at least one tick has passed since
  // acquire.
  void maybePresentForFrame(uint64_t currentFrame) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_readyToPresent && _acquiredAtFrame &&
        *_acquiredAtFrame < currentFrame) {
      _presentLocked();
    }
  }

  void markSubmittedForPresentation() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_textureAcquired) {
      _readyToPresent = true;
    }
  }

  wgpu::Texture getCurrentTexture(uint64_t currentFrame) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (surface) {
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      _textureAcquired = true;
      _readyToPresent = false;
      _acquiredAtFrame = currentFrame;
      return surfaceTexture.texture;
    } else {
      return texture;
    }
  }

  NativeInfo getNativeInfo() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.nativeSurface = nativeSurface, .width = width, .height = height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.width = width, .height = height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return config;
  }

  wgpu::Device getDevice() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return config.device;
  }

private:
  void _configure() {
    if (surface) {
      surface.Configure(&config);
    } else {
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

  // Caller must hold _mutex as unique_lock.
  void _presentLocked() {
    if (surface && _textureAcquired) {
#ifdef __APPLE__
      if (config.device) {
        dawn::native::metal::WaitForCommandsToBeScheduled(config.device.Get());
      }
#endif
      surface.Present();
      _textureAcquired = false;
      _readyToPresent = false;
      _acquiredAtFrame.reset();
    }
  }

  // Caller must hold _mutex as unique_lock.
  void _resetPresentationStateLocked() {
    _textureAcquired = false;
    _readyToPresent = false;
    _acquiredAtFrame.reset();
  }

  mutable std::shared_mutex _mutex;
  void *nativeSurface = nullptr;
  wgpu::Surface surface = nullptr;
  wgpu::Texture texture = nullptr;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;
  int width;
  int height;
  bool _textureAcquired = false;
  bool _readyToPresent = false;
  std::optional<uint64_t> _acquiredAtFrame;
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

  std::shared_ptr<SurfaceInfo> addSurfaceInfo(int id, wgpu::Instance gpu,
                                              int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry[id] = info;
    return info;
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoOrCreate(int id, wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry[id] = info;
    return info;
  }

  // Monotonically increasing tick counter. getCurrentTexture stamps the
  // surface with the value seen at acquisition time; tickAll() presents
  // surfaces whose stamp is strictly less than the current counter, which
  // guarantees the JS render code between two vsyncs has finished.
  uint64_t getCurrentFrame() const { return _frameCounter.load(); }

  void tickAll() {
    auto current = _frameCounter.fetch_add(1, std::memory_order_acq_rel) + 1;
    std::vector<std::shared_ptr<SurfaceInfo>> snapshot;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      snapshot.reserve(_registry.size());
      for (auto &entry : _registry) {
        snapshot.push_back(entry.second);
      }
    }
    for (auto &info : snapshot) {
      info->maybePresentForFrame(current);
    }
  }

  void markSubmittedSurfacesForPresentation() {
    std::vector<std::shared_ptr<SurfaceInfo>> snapshot;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      snapshot.reserve(_registry.size());
      for (auto &entry : _registry) {
        snapshot.push_back(entry.second);
      }
    }
    for (auto &info : snapshot) {
      info->markSubmittedForPresentation();
    }
  }

private:
  SurfaceRegistry() = default;
  mutable std::shared_mutex _mutex;
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
  std::atomic<uint64_t> _frameCounter{0};
};

} // namespace rnwgpu
