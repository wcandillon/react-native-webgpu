#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPipelineLayoutDescriptor {
public:
  wgpu::PipelineLayoutDescriptor *getInstance() { return &_instance; }

  wgpu::PipelineLayoutDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineLayoutDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUPipelineLayoutDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUPipelineLayoutDescriptor>();
    if (value.hasProperty(runtime, "bindGroupLayouts")) {
      auto bindGroupLayouts = value.getProperty(runtime, "bindGroupLayouts");

      else if (bindGroupLayouts.isUndefined()) {
        throw std::runtime_error(
            "Property GPUPipelineLayoutDescriptor::bindGroupLayouts is "
            "required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUPipelineLayoutDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
