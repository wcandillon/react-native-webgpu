#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUShaderStage : public m::HybridObject {
public:
  GPUShaderStage() : HybridObject("GPUShaderStage") {}

public:
  wgpu::ShaderStage Vertex() { return wgpu::ShaderStage::Vertex; };
  wgpu::ShaderStage Fragment() { return wgpu::ShaderStage::Fragment; };
  wgpu::ShaderStage Compute() { return wgpu::ShaderStage::Compute; }

  void loadHybridMethods() override {
    registerHybridGetter("VERTEX", &GPUShaderStage::Vertex, this);
    registerHybridGetter("FRAGMENT", &GPUShaderStage::Fragment, this);
    registerHybridGetter("COMPUTE", &GPUShaderStage::Compute, this);
  }
};
} // namespace rnwgpu