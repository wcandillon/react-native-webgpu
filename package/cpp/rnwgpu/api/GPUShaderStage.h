#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUShaderStage : public m::HybridObject {
public:
  GPUShaderStage() : HybridObject("GPUShaderStage") {}

public:
  double Vertex() { return static_cast<double>(wgpu::ShaderStage::Vertex); };
  double Fragment() {
    return static_cast<double>(wgpu::ShaderStage::Fragment);
  };
  double Compute() { return static_cast<double>(wgpu::ShaderStage::Compute); }

  void loadHybridMethods() override {
    registerHybridGetter("VERTEX", &GPUShaderStage::Vertex, this);
    registerHybridGetter("FRAGMENT", &GPUShaderStage::Fragment, this);
    registerHybridGetter("COMPUTE", &GPUShaderStage::Compute, this);
  }
};
} // namespace rnwgpu