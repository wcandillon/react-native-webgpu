#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::ComputePassDescriptor>> {
  static std::shared_ptr<wgpu::ComputePassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ComputePassDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "timestampWrites")) {
        auto timestampWrites = value.getProperty(runtime, "timestampWrites");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->_instance.label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPUComputePassDescriptor::timestampWrites = %f",
        result->_instance.timestampWrites);
    rnwgpu::Logger::logToConsole("GPUComputePassDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::ComputePassDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
