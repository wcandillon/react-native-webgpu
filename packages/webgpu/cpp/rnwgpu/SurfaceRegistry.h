#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

class SurfaceInfo {
public:
  SurfaceInfo(wgpu::Instance gpu, int width, int height)
      : gpu(gpu), width(width), height(height) {}

  void configure(wgpu::SurfaceConfiguration &newConfig) {
    config = newConfig;
    config.width = width;
    config.height = height;
    if (surface) {
      surface.Configure(&config);
    } else {
      wgpu::TextureDescriptor textureDesc;
      config.usage = wgpu::TextureUsage::RenderAttachment |
                     wgpu::TextureUsage::CopySrc |
                     wgpu::TextureUsage::TextureBinding;
      texture = config.device.CreateTexture(&textureDesc);
    }
  }

  void unconfigure() {
    if (surface) {
      surface.Unconfigure();
    } else {
      texture = nullptr;
    }
  }

  void switchToOffscreen() {
    surface = nullptr;
    wgpu::TextureDescriptor textureDesc;
    config.usage = wgpu::TextureUsage::RenderAttachment |
                   wgpu::TextureUsage::CopySrc |
                   wgpu::TextureUsage::TextureBinding;
    texture = config.device.CreateTexture(&textureDesc);
  }

  void switchToOnscreen(void *newNativeSurface, wgpu::Surface newSurface) {
    nativeSurface = newNativeSurface;
    surface = std::move(newSurface);
    // If we had an offscreen texture we can configure it and flush it
    if (texture != nullptr) {
      surface.Configure(&config);
      // TODO: flush
    }
    texture = nullptr;
  }

  void resize(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
    config.width = width;
    config.height = height;
    configure(config);
  }

  void present() {
    if (surface) {
      surface.Present();
    }
  }

  wgpu::Texture getCurrentTexture() {
    if (surface) {
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      return surfaceTexture.texture;
    } else {
      return texture;
    }
  }

  void *getNativeSurface() { return nativeSurface; }

  wgpu::SurfaceConfiguration &getConfig() { return config; }

  int getWidth() const { return width; }

  int getHeight() const { return height; }

private:
  void *nativeSurface = nullptr;
  wgpu::Surface surface = nullptr;
  wgpu::Texture texture = nullptr;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;
  int width;
  int height;
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
    return _registry[id];
  }

  void removeSurfaceInfo(int id) {
    _registry.erase(id);
  }

  // Thread-safe method to add a new SurfaceInfo
  std::shared_ptr<SurfaceInfo> addSurfaceInfo(int id, wgpu::Instance gpu,
                                              int width, int height) {
    std::shared_ptr<SurfaceInfo> info =
        std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry.emplace(id, info);
    return _registry[id];
  }
  
  std::shared_ptr<SurfaceInfo> getSurfaceInfoOrCreate(int id, wgpu::Instance gpu,
                                                      int width, int height) {
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    return addSurfaceInfo(id, gpu, width, height);
  }

private:
  SurfaceRegistry() = default;
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
};

} // namespace rnwgpu
