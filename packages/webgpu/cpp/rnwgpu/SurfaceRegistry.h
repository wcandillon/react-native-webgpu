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
    _configure();
  }

  void unconfigure() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (surface) {
      surface.Unconfigure();
    } else {
      texture = nullptr;
    }
  }

  void switchToOffscreen() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    surface = nullptr;
    wgpu::TextureDescriptor textureDesc;
    config.usage = wgpu::TextureUsage::RenderAttachment |
                   wgpu::TextureUsage::CopySrc |
                   wgpu::TextureUsage::TextureBinding;
    texture = config.device.CreateTexture(&textureDesc);
  }

  void switchToOnscreen(void *newNativeSurface, wgpu::Surface newSurface) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
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
      surface.Present();
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
    if (surface) {
      surface.Present();
    }
  }

  wgpu::Texture getCurrentTexture() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (surface) {
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      return surfaceTexture.texture;
    } else {
      return texture;
    }
  }

  void *getNativeSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return nativeSurface;
  }

  wgpu::SurfaceConfiguration &getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return config;
  }

  int getWidth() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return width;
  }

  int getHeight() const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return height;
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

  mutable std::shared_mutex _mutex;
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

private:
  SurfaceRegistry() = default;
  mutable std::shared_mutex _mutex;
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
};

} // namespace rnwgpu
