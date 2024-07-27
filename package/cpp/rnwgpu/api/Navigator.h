#pragma once

#include "GPU.h"

namespace rnwgpu {

namespace m = margelo;

class Navigator: public m::HybridObject {
public:
  Navigator(std::shared_ptr<GPU> gpu) : HybridObject("Navigator"), _gpu(gpu) {}

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  void loadHybridMethods() override {
    registerHybridGetter("gpu", &Navigator::getGPU, this);
  }
private:
  std::shared_ptr<GPU> _gpu;
};

} // namespace rnwgpu