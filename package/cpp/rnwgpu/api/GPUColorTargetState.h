#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUColorTargetState {
public:
  wgpu::ColorTargetState *getInstance() { return &_instance; }

private:
  wgpu::ColorTargetState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorTargetState>> {
  static std::shared_ptr<rnwgpu::GPUColorTargetState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUColorTargetState>();
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      }
    }
    if (value.hasProperty(runtime, "blend")) {
      auto blend = value.getProperty(runtime, "blend");
      if (blend.isNumber()) {
        result->_instance.blend = blend.getNumber();
      } else if (blend.isNull() || blend.isUndefined()) {
        throw std::runtime_error(
            "Property GPUColorTargetState::blend is required");
      }
    }
    if (value.hasProperty(runtime, "writeMask")) {
      auto writeMask = value.getProperty(runtime, "writeMask");
      if (writeMask.isNumber()) {
        result->_instance.writeMask = writeMask.getNumber();
      } else if (writeMask.isNull() || writeMask.isUndefined()) {
        throw std::runtime_error(
            "Property GPUColorTargetState::writeMask is required");
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
