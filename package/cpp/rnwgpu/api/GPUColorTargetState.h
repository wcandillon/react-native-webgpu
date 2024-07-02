#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUColorTargetState {
public:
  wgpu::ColorTargetState *getInstance() { return &_instance; }

  wgpu::ColorTargetState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorTargetState>> {
  static std::shared_ptr<rnwgpu::GPUColorTargetState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUColorTargetState>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUColorTargetState::format is required");
        }
      }
      if (value.hasProperty(runtime, "blend")) {
        auto blend = value.getProperty(runtime, "blend");
      }
      if (value.hasProperty(runtime, "writeMask")) {
        auto writeMask = value.getProperty(runtime, "writeMask");

        if (value.hasProperty(runtime, "writeMask")) {
          result->_instance.writeMask = writeMask.getNumber();
        }
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUColorTargetState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
