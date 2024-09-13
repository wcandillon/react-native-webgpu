#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "Canvas.h"

namespace rnwgpu {

class SurfaceRegistry {
  std::unordered_map<int, std::shared_ptr<Canvas>> _registry;
  std::recursive_mutex _registryMutex;

public:
  void addSurface(int contextId, const void *surface, float width, float height);
  void addEmptySurface(int contextId, float width, float height);
  std::shared_ptr<Canvas> getSurface(int contextId);
  void removeSurface(int contextId);
  void updateSurface(int contextId, float width, float height);
};

} // namespace rnwgpu
