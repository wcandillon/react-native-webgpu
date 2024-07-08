#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUFragmentState {
public:
  wgpu::FragmentState *getInstance() { return &_instance; }

  wgpu::FragmentState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUFragmentState>> {
  static std::shared_ptr<rnwgpu::GPUFragmentState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUFragmentState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "targets")) {
        auto targets = value.getProperty(runtime, "targets");

        if (targets.isUndefined()) {
          throw std::runtime_error(
              "Property GPUFragmentState::targets is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUFragmentState::targets is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUFragmentState::targets = %f",
                                 result->_instance.targets);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUFragmentState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
