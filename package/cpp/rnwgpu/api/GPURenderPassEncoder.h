#pragma once

#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderPassEncoder : public m::HybridObject {
public:
  explicit GPURenderPassEncoder(
      std::shared_ptr<wgpu::RenderPassEncoder> instance)
      : HybridObject("GPURenderPassEncoder"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPassEncoder::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::RenderPassEncoder> _instance;
};
} // namespace rnwgpu