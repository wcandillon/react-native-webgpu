#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::Origin2DDictStrict>> {
  static std::shared_ptr<wgpu::Origin2DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::Origin2DDictStrict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "z")) {
        auto z = value.getProperty(runtime, "z");

        if (z.isUndefined()) {
          throw std::runtime_error(
              "Property GPUOrigin2DDictStrict::z is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUOrigin2DDictStrict::z is not defined");
      }
      if (value.hasProperty(runtime, "x")) {
        auto x = value.getProperty(runtime, "x");

        if (x.isNumber()) {
          result->x = static_cast<wgpu::IntegerCoordinate>(x.getNumber());
        }
      }
      if (value.hasProperty(runtime, "y")) {
        auto y = value.getProperty(runtime, "y");

        if (y.isNumber()) {
          result->y = static_cast<wgpu::IntegerCoordinate>(y.getNumber());
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUOrigin2DDictStrict::z = %f", result->z);
    rnwgpu::Logger::logToConsole("GPUOrigin2DDictStrict::x = %f", result->x);
    rnwgpu::Logger::logToConsole("GPUOrigin2DDictStrict::y = %f", result->y);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::Origin2DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
