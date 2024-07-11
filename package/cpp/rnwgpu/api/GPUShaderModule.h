#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUShaderModule : public m::HybridObject {
public:
  explicit GPUShaderModule(wgpu::ShaderModule instance, std::string label)
      : HybridObject("GPUShaderModule"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUShaderModule::getBrand, this);

    registerHybridGetter("label", &GPUShaderModule::getLabel, this);
  }

private:
  wgpu::ShaderModule _instance;
  std::string _label;
};
} // namespace rnwgpu