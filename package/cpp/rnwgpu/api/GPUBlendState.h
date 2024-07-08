#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::BlendState>> {
  static std::shared_ptr<wgpu::BlendState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::BlendState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "color")) {
        auto color = value.getProperty(runtime, "color");

        if (color.isUndefined()) {
          throw std::runtime_error("Property GPUBlendState::color is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBlendState::color is not defined");
      }
      if (value.hasProperty(runtime, "alpha")) {
        auto alpha = value.getProperty(runtime, "alpha");

        if (alpha.isUndefined()) {
          throw std::runtime_error("Property GPUBlendState::alpha is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBlendState::alpha is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUBlendState::color = %f", result->color);
    rnwgpu::Logger::logToConsole("GPUBlendState::alpha = %f", result->alpha);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::BlendState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
