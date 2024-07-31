#pragma once

#include <future>
#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUCompilationInfo.h"

namespace rnwgpu {

namespace m = margelo;

class GPUShaderModule : public m::HybridObject {
public:
  explicit GPUShaderModule(wgpu::ShaderModule instance,
                           std::shared_ptr<AsyncRunner> async,
                           std::string label)
      : HybridObject("GPUShaderModule"), _instance(instance), _async(async),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUCompilationInfo>> getCompilationInfo();

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUShaderModule::getBrand, this);
    registerHybridMethod("getCompilationInfo",
                         &GPUShaderModule::getCompilationInfo, this);

    registerHybridGetter("label", &GPUShaderModule::getLabel, this);
    registerHybridSetter("label", &GPUShaderModule::setLabel, this);
  }

  inline const wgpu::ShaderModule get() { return _instance; }

private:
  wgpu::ShaderModule _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
};

} // namespace rnwgpu