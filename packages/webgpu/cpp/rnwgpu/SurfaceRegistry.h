#pragma once

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "SurfaceBridge.h"

namespace rnwgpu {

class SurfaceRegistry {
public:
  static SurfaceRegistry &getInstance() {
    static SurfaceRegistry instance;
    return instance;
  }

  SurfaceRegistry(const SurfaceRegistry &) = delete;
  SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;

  std::shared_ptr<SurfaceBridge> getSurfaceInfo(int id) {
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

  std::shared_ptr<SurfaceBridge>
  getSurfaceInfoOrCreate(int id, GPUWithLock gpu) {

    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    auto bridge = createSurfaceBridge(gpu);
    _registry[id] = bridge;
    return bridge;
  }

private:
  SurfaceRegistry() = default;
  mutable std::shared_mutex _mutex;
  std::unordered_map<int, std::shared_ptr<SurfaceBridge>> _registry;
};

} // namespace rnwgpu
