#pragma once

#include <memory>
#include <unordered_map>
#include <shared_mutex>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct SurfaceInfo {
  void *surface;
  float width;
  float height;

  SurfaceInfo(void *surface, float width, float height)
      : surface(surface), width(width), height(height) {}

  void setClientWidth(float width) { this->width = width; }

  void setClientHeight(float height) { this->height = height; }

  float getWidth() { return width; }

  float getHeight() { return height; }
};

class SurfaceRegistry {
private:
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
  mutable std::shared_mutex _mutex;

  // Private constructor to prevent instantiation
  SurfaceRegistry() {}

public:
  // Delete copy constructor and assignment operator
  SurfaceRegistry(const SurfaceRegistry&) = delete;
  SurfaceRegistry& operator=(const SurfaceRegistry&) = delete;

  // Static method to get the singleton instance
  static SurfaceRegistry& getInstance() {
    static SurfaceRegistry instance;
    return instance;
  }

  void addSurface(const int contextId, void *surface, float width,
                  float height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry[contextId] =
        std::make_shared<SurfaceInfo>(surface, width, height);
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

  void updateSurface(const int contextId, float width, float height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(contextId);
    if (it != _registry.end()) {
      it->second->setClientWidth(width);
      it->second->setClientHeight(height);
    }
  }
};

} // namespace rnwgpu
