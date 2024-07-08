#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::ColorDict>> {
  static std::shared_ptr<wgpu::ColorDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ColorDict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "r")) {
        auto r = value.getProperty(runtime, "r");

        if (r.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::r is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::r is not defined");
      }
      if (value.hasProperty(runtime, "g")) {
        auto g = value.getProperty(runtime, "g");

        if (g.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::g is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::g is not defined");
      }
      if (value.hasProperty(runtime, "b")) {
        auto b = value.getProperty(runtime, "b");

        if (b.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::b is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::b is not defined");
      }
      if (value.hasProperty(runtime, "a")) {
        auto a = value.getProperty(runtime, "a");

        if (a.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::a is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::a is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUColorDict::r = %f", result->r);
    rnwgpu::Logger::logToConsole("GPUColorDict::g = %f", result->g);
    rnwgpu::Logger::logToConsole("GPUColorDict::b = %f", result->b);
    rnwgpu::Logger::logToConsole("GPUColorDict::a = %f", result->a);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::ColorDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
