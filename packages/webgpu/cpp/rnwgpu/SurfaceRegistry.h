#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct SurfaceInfo {
  void *surface;
  int width;
  int height;
  wgpu::Texture texture;

  SurfaceInfo(void *surface, int width, int height, wgpu::Texture texture)
      : surface(surface), width(width), height(height), texture(texture) {}
};

class SurfaceRegistry {
private:
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
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

  void addSurface(const int contextId, void *surface, int width, int height,
                  wgpu::Texture texture) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry[contextId] =
        std::make_shared<SurfaceInfo>(surface, width, height, texture);
  }

  std::shared_ptr<SurfaceInfo> getSurface(const int contextId) const {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      return it->second;
    }
    return nullptr;
  }

  void removeSurface(const int contextId) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.erase(contextId);
  }

  void updateSurface(const int contextId, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      it->second->width = width;
      it->second->height = height;
    }
  }
};

} // namespace rnwgpu
