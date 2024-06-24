#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBuffer : public m::HybridObject {
public:
  GPUBuffer(std::shared_ptr<wgpu::Buffer> instance)
      : HybridObject("GPUBuffer"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBuffer::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::Buffer> _instance;
};
} // namespace rnwgpu