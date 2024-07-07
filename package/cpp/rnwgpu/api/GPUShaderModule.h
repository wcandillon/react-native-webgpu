#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>
// #include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUShaderModule : public m::HybridObject {
public:
  explicit GPUShaderModule(std::shared_ptr<wgpu::ShaderModule> instance)
      : HybridObject("GPUShaderModule"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUShaderModule::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::ShaderModule> _instance;
};
} // namespace rnwgpu