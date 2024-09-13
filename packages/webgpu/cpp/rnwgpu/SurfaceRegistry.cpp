#include "SurfaceRegistry.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

void SurfaceRegistry::addSurface(
  const int contextId, void *surface, const float width, const float height) {
  auto lock = std::unique_lock<std::recursive_mutex>(_registryMutex);
  if (_registry.find(contextId) == _registry.end()) {
    _registry[contextId] = std::make_shared<Canvas>(surface, width, height, contextId);
  } else {
    auto canvas = _registry[contextId];
    canvas->setSurface(surface);
  }
}

void SurfaceRegistry::addEmptySurface(const int contextId, const float width, const float height) {
  auto lock = std::unique_lock<std::recursive_mutex>(_registryMutex);
  _registry[contextId] = std::make_shared<Canvas>(nullptr, width, height, contextId);
}

std::shared_ptr<Canvas> SurfaceRegistry::getSurface(const int contextId) {
  auto lock = std::unique_lock<std::recursive_mutex>(_registryMutex);
  return _registry[contextId];
}

void SurfaceRegistry::removeSurface(const int contextId) {
  auto lock = std::unique_lock<std::recursive_mutex>(_registryMutex);
  _registry.erase(contextId);
}

void SurfaceRegistry::updateSurface(const int contextId, const float width, const float height) {
  auto lock = std::unique_lock<std::recursive_mutex>(_registryMutex);
  if (_registry.find(contextId) != _registry.end()) {
    _registry[contextId]->setClientWidth(width);
    _registry[contextId]->setClientHeight(height);
  }
}

} // namespace rnwgpu
