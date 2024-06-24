#pragma once

#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUPipelineBase : public m::HybridObject {
public:
  explicit GPUPipelineBase(std::shared_ptr<wgpu::PipelineBase> instance)
      : HybridObject("GPUPipelineBase"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUPipelineBase::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::PipelineBase> _instance;
};
} // namespace rnwgpu