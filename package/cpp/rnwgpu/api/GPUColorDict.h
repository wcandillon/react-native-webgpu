#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUColorDict {
public:
  wgpu::ColorDict *getInstance() { return &_instance; }

  wgpu::ColorDict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorDict>> {
  static std::shared_ptr<rnwgpu::GPUColorDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUColorDict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "r")) {
        auto r = value.getProperty(runtime, "r");

        if (r.isNumber()) {
          result->_instance.r = r.getNumber();
        }

        if (r.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::r is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::r is not defined");
      }
      if (value.hasProperty(runtime, "g")) {
        auto g = value.getProperty(runtime, "g");

        if (g.isNumber()) {
          result->_instance.g = g.getNumber();
        }

        if (g.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::g is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::g is not defined");
      }
      if (value.hasProperty(runtime, "b")) {
        auto b = value.getProperty(runtime, "b");

        if (b.isNumber()) {
          result->_instance.b = b.getNumber();
        }

        if (b.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::b is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::b is not defined");
      }
      if (value.hasProperty(runtime, "a")) {
        auto a = value.getProperty(runtime, "a");

        if (a.isNumber()) {
          result->_instance.a = a.getNumber();
        }

        if (a.isUndefined()) {
          throw std::runtime_error("Property GPUColorDict::a is required");
        }
      } else {
        throw std::runtime_error("Property GPUColorDict::a is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUColorDict::r = %f", result->_instance.r);
    rnwgpu::Logger::logToConsole("GPUColorDict::g = %f", result->_instance.g);
    rnwgpu::Logger::logToConsole("GPUColorDict::b = %f", result->_instance.b);
    rnwgpu::Logger::logToConsole("GPUColorDict::a = %f", result->_instance.a);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUColorDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
