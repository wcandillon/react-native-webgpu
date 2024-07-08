#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePipeline : public m::HybridObject {
public:
  explicit GPUComputePipeline(std::shared_ptr<wgpu::ComputePipeline> instance)
      : HybridObject("GPUComputePipeline"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePipeline::getBrand, this);

    registerHybridGetter("label", &GPUComputePipeline::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::ComputePipeline> _instance;
};
} // namespace rnwgpu