#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>
// #include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandBuffer : public m::HybridObject {
public:
  explicit GPUCommandBuffer(std::shared_ptr<wgpu::CommandBuffer> instance)
      : HybridObject("GPUCommandBuffer"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandBuffer::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::CommandBuffer> _instance;
};
} // namespace rnwgpu