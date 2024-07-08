#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::MultisampleState>> {
  static std::shared_ptr<wgpu::MultisampleState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::MultisampleState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "count")) {
        auto count = value.getProperty(runtime, "count");

        if (count.isNumber()) {
          result->count = static_cast<wgpu::Size32>(count.getNumber());
        }
      }
      if (value.hasProperty(runtime, "mask")) {
        auto mask = value.getProperty(runtime, "mask");

        if (mask.isNumber()) {
          result->mask = static_cast<wgpu::SampleMask>(mask.getNumber());
        }
      }
      if (value.hasProperty(runtime, "alphaToCoverageEnabled")) {
        auto alphaToCoverageEnabled =
            value.getProperty(runtime, "alphaToCoverageEnabled");
      }
    }
    rnwgpu::Logger::logToConsole("GPUMultisampleState::count = %f",
                                 result->count);
    rnwgpu::Logger::logToConsole("GPUMultisampleState::mask = %f",
                                 result->mask);
    rnwgpu::Logger::logToConsole(
        "GPUMultisampleState::alphaToCoverageEnabled = %f",
        result->alphaToCoverageEnabled);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::MultisampleState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
