#pragma once

#include <memory>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

#include "Canvas.h"

namespace rnwgpu {

class SurfaceRegistry {
  std::unordered_map<int, std::shared_ptr<Canvas>> _registry;

public:
  void addSurface(const int contextId, void *surface, float width,
                  float height) {
    _registry[contextId] = std::make_shared<Canvas>(
        reinterpret_cast<uint64_t>(surface), width, height);
  }

  std::shared_ptr<Canvas> getSurface(const int contextId) {
    return _registry[contextId];
  }

  void removeSurface(const int contextId) { _registry.erase(contextId); }

  void updateSurface(const int contextId, float width, float height) {
    if (_registry.find(contextId) != _registry.end()) {
      _registry[contextId]->setClientWidth(width);
      _registry[contextId]->setClientHeight(height);
    }
  }
};

} // namespace rnwgpu
