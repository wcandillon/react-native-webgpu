#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPipelineDescriptorBase {
public:
  wgpu::PipelineDescriptorBase *getInstance() { return &_instance; }

private:
  wgpu::PipelineDescriptorBase _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineDescriptorBase>> {
  static std::shared_ptr<rnwgpu::GPUPipelineDescriptorBase>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUPipelineDescriptorBase>();
    if (value.hasProperty(runtime, "layout")) {
      auto layout = value.getProperty(runtime, "layout");
      if (layout.isNumber()) {
        result->_instance.layout = layout.getNumber();
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUPipelineDescriptorBase> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
