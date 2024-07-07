#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBindGroupLayout : public m::HybridObject {
public:
  explicit GPUBindGroupLayout(std::shared_ptr<wgpu::BindGroupLayout> instance)
      : HybridObject("GPUBindGroupLayout"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBindGroupLayout::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::BindGroupLayout> _instance;
};
} // namespace rnwgpu