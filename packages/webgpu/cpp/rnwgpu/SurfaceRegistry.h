#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct ISize {
  int width;
  int height;
};

struct SurfaceInfo {
  void *nativeSurface = nullptr;
  wgpu::Surface surface;
  int width;
  int height;
  wgpu::Texture texture;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;

  void flushTextureToSurface() {
    // 1.a flush texture to the onscreen surface
    if (texture) {
      wgpu::CommandEncoderDescriptor encoderDesc;
      auto device = config.device;
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
      texture = nullptr;
    }
  }
};

class SurfaceRegistry {
private:
  std::unordered_map<int, SurfaceInfo> _registry;
  mutable std::shared_mutex _mutex;

  // Private constructor to prevent instantiation
  SurfaceRegistry() {}

  void updateSurface(const int contextId, SurfaceInfo &info) {
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      it->second = info;
    }
  }

  // bool hasOnScreenSurface(const int contextId) const {
  //   std::shared_lock<std::shared_mutex> lock(_mutex);
  //   auto it = _registry.find(contextId);
  //   if (it != _registry.end()) {
  //     return it->second.nativeSurface != nullptr;
  //   }
  //   return false;
  // }

  bool hasSurfaceInfo(const int contextId) const {
    return _registry.find(contextId) != _registry.end();
  }

public:
  // Delete copy constructor and assignment operator
  SurfaceRegistry(const SurfaceRegistry &) = delete;
  SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;

  // Static method to get the singleton instance
  static SurfaceRegistry &getInstance() {
    static SurfaceRegistry instance;
    return instance;
  }

  void removeSurface(const int contextId) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.erase(contextId);
  }

  std::optional<SurfaceInfo> getSurfaceMaybe(const int contextId) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  SurfaceInfo getSurface(const int contextId) const {
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      return it->second;
    }
    throw std::out_of_range("Surface not found");
  }

  void setSize(const int contextId, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      it->second.width = width;
      it->second.height = height;
    }
  }

  void
  configureOffscreenSurface(const int contextId, wgpu::Instance gpu,
                            wgpu::Texture texture,
                            wgpu::SurfaceConfiguration surfaceConfiguration) {
    SurfaceInfo info;
    info.width = surfaceConfiguration.width;
    info.height = surfaceConfiguration.height;
    info.texture = texture;
    info.gpu = gpu;
    info.config = surfaceConfiguration;
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry[contextId] = info;
  }

  void createSurface(const int contextId, void *nativeSurface, int width,
                     int height,
                     std::shared_ptr<PlatformContext> platformContext) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    // 1. The scene has already be drawn offscreen
    if (hasSurfaceInfo(contextId)) {
      auto info = getSurface(contextId);
      auto surface =
          platformContext->makeSurface(info.gpu, nativeSurface, width, height);
      info.config.usage = info.config.usage | wgpu::TextureUsage::CopyDst;
      surface.Configure(&info.config);
      info.nativeSurface = nativeSurface;
      info.surface = surface;
      info.width = width;
      info.height = height;
      info.flushTextureToSurface();
      surface.Present();
      updateSurface(contextId, info);
    } else {
      // 2. The scene has not been drawn offscreen yet, we will draw onscreen
      // directly
      rnwgpu::SurfaceInfo info;
      info.nativeSurface = nativeSurface;
      info.surface =
          platformContext->makeSurface(info.gpu, nativeSurface, width, height);
      info.width = width;
      info.height = height;
      _registry[contextId] = info;
    }
  }
};

} // namespace rnwgpu
