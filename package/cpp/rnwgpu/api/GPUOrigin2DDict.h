#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::Origin2DDict>> {
  static std::shared_ptr<wgpu::Origin2DDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::Origin2DDict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "x")) {
        auto x = value.getProperty(runtime, "x");

        if (x.isNumber()) {
          result->_instance.x =
              static_cast<wgpu::IntegerCoordinate>(x.getNumber());
        }
      }
      if (value.hasProperty(runtime, "y")) {
        auto y = value.getProperty(runtime, "y");

        if (y.isNumber()) {
          result->_instance.y =
              static_cast<wgpu::IntegerCoordinate>(y.getNumber());
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUOrigin2DDict::x = %f",
                                 result->_instance.x);
    rnwgpu::Logger::logToConsole("GPUOrigin2DDict::y = %f",
                                 result->_instance.y);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::Origin2DDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
