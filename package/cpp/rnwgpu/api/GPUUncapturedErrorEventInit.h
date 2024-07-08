#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUUncapturedErrorEventInit {
public:
  wgpu::UncapturedErrorEventInit *getInstance() { return &_instance; }

  wgpu::UncapturedErrorEventInit _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>> {
  static std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUUncapturedErrorEventInit>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "error")) {
        auto error = value.getProperty(runtime, "error");

        if (error.isUndefined()) {
          throw std::runtime_error(
              "Property GPUUncapturedErrorEventInit::error is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUUncapturedErrorEventInit::error is not defined");
      }
      if (value.hasProperty(runtime, "bubbles")) {
        auto bubbles = value.getProperty(runtime, "bubbles");
      }
      if (value.hasProperty(runtime, "cancelable")) {
        auto cancelable = value.getProperty(runtime, "cancelable");
      }
      if (value.hasProperty(runtime, "composed")) {
        auto composed = value.getProperty(runtime, "composed");
      }
    }
    rnwgpu::Logger::logToConsole("GPUUncapturedErrorEventInit::error = %f",
                                 result->_instance.error);
    rnwgpu::Logger::logToConsole("GPUUncapturedErrorEventInit::bubbles = %f",
                                 result->_instance.bubbles);
    rnwgpu::Logger::logToConsole("GPUUncapturedErrorEventInit::cancelable = %f",
                                 result->_instance.cancelable);
    rnwgpu::Logger::logToConsole("GPUUncapturedErrorEventInit::composed = %f",
                                 result->_instance.composed);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
