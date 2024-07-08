#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::RequestAdapterOptions>> {
  static std::shared_ptr<wgpu::RequestAdapterOptions>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::RequestAdapterOptions>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "powerPreference")) {
        auto powerPreference = value.getProperty(runtime, "powerPreference");
      }
      if (value.hasProperty(runtime, "forceFallbackAdapter")) {
        auto forceFallbackAdapter =
            value.getProperty(runtime, "forceFallbackAdapter");
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPURequestAdapterOptions::powerPreference = %f",
        result->powerPreference);
    rnwgpu::Logger::logToConsole(
        "GPURequestAdapterOptions::forceFallbackAdapter = %f",
        result->forceFallbackAdapter);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::RequestAdapterOptions> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
