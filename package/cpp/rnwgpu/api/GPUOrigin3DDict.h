#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUOrigin3DDict {
public:
  wgpu::Origin3DDict *getInstance() { return &_instance; }

  wgpu::Origin3DDict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin3DDict>> {
  static std::shared_ptr<rnwgpu::GPUOrigin3DDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUOrigin3DDict>();
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
      if (value.hasProperty(runtime, "z")) {
        auto z = value.getProperty(runtime, "z");

        if (z.isNumber()) {
          result->_instance.z =
              static_cast<wgpu::IntegerCoordinate>(z.getNumber());
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUOrigin3DDict::x = %f",
                                 result->_instance.x);
    rnwgpu::Logger::logToConsole("GPUOrigin3DDict::y = %f",
                                 result->_instance.y);
    rnwgpu::Logger::logToConsole("GPUOrigin3DDict::z = %f",
                                 result->_instance.z);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin3DDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
