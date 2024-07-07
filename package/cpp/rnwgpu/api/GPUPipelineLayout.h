#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>
// #include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUPipelineLayout : public m::HybridObject {
public:
  explicit GPUPipelineLayout(std::shared_ptr<wgpu::PipelineLayout> instance)
      : HybridObject("GPUPipelineLayout"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUPipelineLayout::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::PipelineLayout> _instance;
};
} // namespace rnwgpu