#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct SurfaceInfo {
  void *nativeSurface = nullptr;
  wgpu::Surface surface;
  int width;
  int height;
  wgpu::Texture texture;
  wgpu::Device device;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;

  void flush() {
    // 1.a flush texture to the onscreen surface
    if (texture) {
      wgpu::CommandEncoderDescriptor encoderDesc;
      wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&encoderDesc);

      wgpu::ImageCopyTexture sourceTexture = {};
      sourceTexture.texture = texture;

      wgpu::ImageCopyTexture destinationTexture = {};
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
      // TODO: info->texture = nullptr;
    }
  }
};

class SurfaceRegistry {
private:
  std::unordered_map<int, SurfaceInfo> _registry;
  mutable std::shared_mutex _mutex;

  // Private constructor to prevent instantiation
  SurfaceRegistry() {}

public:
  // Delete copy constructor and assignment operator
  SurfaceRegistry(const SurfaceRegistry &) = delete;
  SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;

  // Static method to get the singleton instance
  static SurfaceRegistry &getInstance() {
    static SurfaceRegistry instance;
    return instance;
  }

  bool hasOnScreenSurface(const int contextId) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      return it->second.nativeSurface != nullptr;
    }
    return false;
  }

  void addSurface(const int contextId, SurfaceInfo &info) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry[contextId] = info;
  }

  void addIfEmptySurface(const int contextId, SurfaceInfo &info) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_registry.find(contextId) == _registry.end()) {
      _registry[contextId] = info;
    }
  }

  bool hasSurfaceInfo(const int contextId) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _registry.find(contextId) != _registry.end();
  }

  SurfaceInfo getSurface(const int contextId) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      return it->second;
    }
    throw std::out_of_range("Surface not found");
  }

  void removeSurface(const int contextId) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.erase(contextId);
  }

  void updateSurface(const int contextId, SurfaceInfo &info) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      it->second = info;
    }
  }
};

} // namespace rnwgpu
