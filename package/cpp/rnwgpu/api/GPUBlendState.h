#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUBlendState {
public:
  wgpu::BlendState *getInstance() { return &_instance; }

  wgpu::BlendState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendState>> {
  static std::shared_ptr<rnwgpu::GPUBlendState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBlendState>();
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
    rnwgpu::Logger::logToConsole("GPUBlendState::color = %f",
                                 result->_instance.color);
    rnwgpu::Logger::logToConsole("GPUBlendState::alpha = %f",
                                 result->_instance.alpha);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
