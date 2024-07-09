#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUShaderModuleCompilationHint {
public:
  wgpu::ShaderModuleCompilationHint *getInstance() { return &_instance; }

  wgpu::ShaderModuleCompilationHint _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUShaderModuleCompilationHint>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "entryPoint")) {
        auto entryPoint = value.getProperty(runtime, "entryPoint");

        if (entryPoint.isUndefined()) {
          throw std::runtime_error(
              "Property GPUShaderModuleCompilationHint::entryPoint is "
              "required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUShaderModuleCompilationHint::entryPoint is not "
            "defined");
      }
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPUShaderModuleCompilationHint::entryPoint = %f",
        result->_instance.entryPoint);
    rnwgpu::Logger::logToConsole("GPUShaderModuleCompilationHint::layout = %f",
                                 result->_instance.layout);
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
