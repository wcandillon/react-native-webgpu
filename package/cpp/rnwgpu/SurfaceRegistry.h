#pragma once

#include <memory>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct SurfaceData {
  float width = 0;
  float height = 0;
  void *surface;
};

class SurfaceRegistry {
  std::unordered_map<int, std::shared_ptr<SurfaceData>> _registry;

public:
  void addSurface(const int contextId, const SurfaceData &data) {
    _registry[contextId] = std::make_shared<SurfaceData>(data);
  }
  std::shared_ptr<SurfaceData> getSurface(const int contextId) {
    return _registry[contextId];
  }
  void removeSurface(const int contextId) { _registry.erase(contextId); }
};

} // namespace rnwgpu
