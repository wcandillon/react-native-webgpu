#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUShaderModuleCompilationHint {
public:
  wgpu::ShaderModuleCompilationHint *getInstance() { return &_instance; }

private:
  wgpu::ShaderModuleCompilationHint _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUShaderModuleCompilationHint>();
    if (value.hasProperty(runtime, "entryPoint")) {
      auto entryPoint = value.getProperty(runtime, "entryPoint");
      if (entryPoint.isNumber()) {
        result->_instance.entryPoint = entryPoint.getNumber();
      }
    }
    if (value.hasProperty(runtime, "layout")) {
      auto layout = value.getProperty(runtime, "layout");
      if (layout.isNumber()) {
        result->_instance.layout = layout.getNumber();
      } else if (layout.isNull() || layout.isUndefined()) {
        throw std::runtime_error(
            "Property GPUShaderModuleCompilationHint::layout is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
