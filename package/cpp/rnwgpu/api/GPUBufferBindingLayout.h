#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::BufferBindingLayout>> {
  static std::shared_ptr<wgpu::BufferBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::BufferBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "type")) {
        auto type = value.getProperty(runtime, "type");
      }
      if (value.hasProperty(runtime, "hasDynamicOffset")) {
        auto hasDynamicOffset = value.getProperty(runtime, "hasDynamicOffset");
      }
      if (value.hasProperty(runtime, "minBindingSize")) {
        auto minBindingSize = value.getProperty(runtime, "minBindingSize");

        if (minBindingSize.isNumber()) {
          result->minBindingSize = minBindingSize.getNumber();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUBufferBindingLayout::type = %f",
                                 result->type);
    rnwgpu::Logger::logToConsole(
        "GPUBufferBindingLayout::hasDynamicOffset = %f",
        result->hasDynamicOffset);
    rnwgpu::Logger::logToConsole("GPUBufferBindingLayout::minBindingSize = %f",
                                 result->minBindingSize);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::BufferBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
