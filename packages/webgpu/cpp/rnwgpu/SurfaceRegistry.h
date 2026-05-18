#pragma once

#include <cstdio>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#include "webgpu/webgpu_cpp.h"

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
    if (surface) {
      surface.Unconfigure();
    }
  }

  void setSurface(void *newNativeSurface, wgpu::Surface newSurface) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    nativeSurface = newNativeSurface;
    surface = std::move(newSurface);
    fprintf(stderr,
            "[react-native-wgpu] setSurface(width=%d, height=%d, "
            "deviceConfigured=%s, surfaceValid=%s)\n",
            width, height, config.device != nullptr ? "yes" : "no",
            surface ? "yes" : "no");
    if (config.device != nullptr) {
      _configure();
    }
  }

  void clearSurface() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    surface = nullptr;
    nativeSurface = nullptr;
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
    if (!surface) {
      fprintf(stderr, "[react-native-wgpu] getCurrentTexture: surface is "
                      "NULL (setSurface never called)\n");
      _lastTextureStatus = wgpu::SurfaceGetCurrentTextureStatus::Error;
      _lastErrorWasNullSurface = true;
      return nullptr;
    }
    _lastErrorWasNullSurface = false;
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    _lastTextureStatus = surfaceTexture.status;
    fprintf(stderr,
            "[react-native-wgpu] getCurrentTexture: status=%d, "
            "textureValid=%s\n",
            static_cast<int>(surfaceTexture.status),
            surfaceTexture.texture ? "yes" : "no");
    return surfaceTexture.texture;
  }

  bool wasLastErrorNullSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _lastErrorWasNullSurface;
  }

  wgpu::SurfaceGetCurrentTextureStatus getLastTextureStatus() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _lastTextureStatus;
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
    fprintf(stderr,
            "[react-native-wgpu] _configure(width=%d, height=%d, "
            "surfaceValid=%s, deviceValid=%s)\n",
            config.width, config.height, surface ? "yes" : "no",
            config.device != nullptr ? "yes" : "no");
    if (surface) {
      surface.Configure(&config);
    }
  }

  mutable std::shared_mutex _mutex;
  void *nativeSurface = nullptr;
  wgpu::Surface surface = nullptr;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;
  int width;
  int height;
  wgpu::SurfaceGetCurrentTextureStatus _lastTextureStatus =
      wgpu::SurfaceGetCurrentTextureStatus::Error;
  bool _lastErrorWasNullSurface = false;
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
