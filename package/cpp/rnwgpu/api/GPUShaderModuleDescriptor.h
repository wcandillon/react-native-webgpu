#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUShaderModuleDescriptor {
public:
  wgpu::ShaderModuleDescriptor *getInstance() { return &_instance; }

private:
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
      if (code.isNumber()) {
        result->_instance.code = code.getNumber();
      }
    }
    if (value.hasProperty(runtime, "sourceMap")) {
      auto sourceMap = value.getProperty(runtime, "sourceMap");
      if (sourceMap.isNumber()) {
        result->_instance.sourceMap = sourceMap.getNumber();
      }
    }
    if (value.hasProperty(runtime, "compilationHints")) {
      auto compilationHints = value.getProperty(runtime, "compilationHints");
      if (compilationHints.isNumber()) {
        result->_instance.compilationHints = compilationHints.getNumber();
      } else if (compilationHints.isNull() || compilationHints.isUndefined()) {
        throw std::runtime_error(
            "Property GPUShaderModuleDescriptor::compilationHints is required");
      }
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
