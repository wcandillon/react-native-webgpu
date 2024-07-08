#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePassEncoder : public m::HybridObject {
public:
  explicit GPUComputePassEncoder(
      std::shared_ptr<wgpu::ComputePassEncoder> instance)
      : HybridObject("GPUComputePassEncoder"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePassEncoder::getBrand, this);

    registerHybridGetter("label", &GPUComputePassEncoder::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::ComputePassEncoder> _instance;
};
} // namespace rnwgpu