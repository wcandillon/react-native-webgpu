#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUMultisampleState {
public:
  wgpu::MultisampleState *getInstance() { return &_instance; }

  wgpu::MultisampleState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUMultisampleState>> {
  static std::shared_ptr<rnwgpu::GPUMultisampleState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUMultisampleState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "count")) {
        auto count = value.getProperty(runtime, "count");

        if (count.isNumber()) {
          result->_instance.count =
              static_cast<wgpu::Size32>(count.getNumber());
        }
      }
      if (value.hasProperty(runtime, "mask")) {
        auto mask = value.getProperty(runtime, "mask");

        if (mask.isNumber()) {
          result->_instance.mask =
              static_cast<wgpu::SampleMask>(mask.getNumber());
        }
      }
      if (value.hasProperty(runtime, "alphaToCoverageEnabled")) {
        auto alphaToCoverageEnabled =
            value.getProperty(runtime, "alphaToCoverageEnabled");
      }
    }
    rnwgpu::Logger::logToConsole("GPUMultisampleState::count = %f",
                                 result->_instance.count);
    rnwgpu::Logger::logToConsole("GPUMultisampleState::mask = %f",
                                 result->_instance.mask);
    rnwgpu::Logger::logToConsole(
        "GPUMultisampleState::alphaToCoverageEnabled = %f",
        result->_instance.alphaToCoverageEnabled);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUMultisampleState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
