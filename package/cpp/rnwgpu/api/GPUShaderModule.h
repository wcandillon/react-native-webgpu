#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUShaderModule : public m::HybridObject {
public:
  explicit GPUShaderModule(wgpu::ShaderModule instance,
                           std::shared_ptr<AsyncRunner> async,
                           std::string label)
      : HybridObject("GPUShaderModule"), _instance(std::move(instance)),
        _async(std::move(async)), _label(std::move(label)) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUShaderModule::getBrand, this);

    registerHybridGetter("label", &GPUShaderModule::getLabel, this);
  }

  inline const wgpu::ShaderModule get() { return _instance; }

private:
  wgpu::ShaderModule _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
};

} // namespace rnwgpu