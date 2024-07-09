#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUOrigin2DDictStrict {
public:
  wgpu::Origin2DDictStrict *getInstance() { return &_instance; }

  wgpu::Origin2DDictStrict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict>> {
  static std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUOrigin2DDictStrict>();
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
    rnwgpu::Logger::logToConsole("GPUOrigin2DDictStrict::z = %f",
                                 result->_instance.z);
    rnwgpu::Logger::logToConsole("GPUOrigin2DDictStrict::x = %f",
                                 result->_instance.x);
    rnwgpu::Logger::logToConsole("GPUOrigin2DDictStrict::y = %f",
                                 result->_instance.y);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
