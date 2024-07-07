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

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUShaderModuleDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "code")) {
        auto code = value.getProperty(runtime, "code");

        if (code.isUndefined()) {
          throw std::runtime_error(
              "Property GPUShaderModuleDescriptor::code is required");
        }
      }
      if (value.hasProperty(runtime, "sourceMap")) {
        auto sourceMap = value.getProperty(runtime, "sourceMap");

        if (sourceMap.isUndefined()) {
          throw std::runtime_error(
              "Property GPUShaderModuleDescriptor::sourceMap is required");
        }
      }
      if (value.hasProperty(runtime, "compilationHints")) {
        auto compilationHints = value.getProperty(runtime, "compilationHints");
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
