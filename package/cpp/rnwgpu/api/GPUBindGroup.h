#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>
// #include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBindGroup : public m::HybridObject {
public:
  explicit GPUBindGroup(std::shared_ptr<wgpu::BindGroup> instance)
      : HybridObject("GPUBindGroup"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBindGroup::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::BindGroup> _instance;
};
} // namespace rnwgpu