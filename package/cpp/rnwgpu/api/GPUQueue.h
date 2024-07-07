#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>
// #include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUQueue : public m::HybridObject {
public:
  explicit GPUQueue(std::shared_ptr<wgpu::Queue> instance)
      : HybridObject("GPUQueue"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQueue::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::Queue> _instance;
};
} // namespace rnwgpu