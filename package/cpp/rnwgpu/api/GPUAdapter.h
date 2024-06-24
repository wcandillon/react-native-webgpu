#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapter : public m::HybridObject {
public:
  explicit GPUAdapter(std::shared_ptr<wgpu::Adapter> instance)
      : HybridObject("GPUAdapter"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapter::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::Adapter> _instance;
};
} // namespace rnwgpu