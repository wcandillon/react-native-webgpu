#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUShaderModuleDescriptor {
public:
  wgpu::ShaderModuleDescriptor *getInstance() { return &_instance; }

  wgpu::ShaderModuleDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUShaderModuleDescriptor>();
    if (value.hasProperty(runtime, "code")) {
      auto code = value.getProperty(runtime, "code");

      else if (code.isUndefined()) {
        throw std::runtime_error(
            "Property GPUShaderModuleDescriptor::code is required");
      }
    }
    if (value.hasProperty(runtime, "sourceMap")) {
      auto sourceMap = value.getProperty(runtime, "sourceMap");

      else if (sourceMap.isUndefined()) {
        throw std::runtime_error(
            "Property GPUShaderModuleDescriptor::sourceMap is required");
      }
    }
    if (value.hasProperty(runtime, "compilationHints")) {
      auto compilationHints = value.getProperty(runtime, "compilationHints");
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
