#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandEncoder : public m::HybridObject {
public:
  GPUCommandEncoder(std::shared_ptr<wgpu::CommandEncoder> instance)
      : HybridObject("GPUCommandEncoder"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandEncoder::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::CommandEncoder> _instance;
};
} // namespace rnwgpu