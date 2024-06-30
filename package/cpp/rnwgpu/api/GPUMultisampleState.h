#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUMultisampleState {
public:
  wgpu::MultisampleState *getInstance() { return &_instance; }

private:
  wgpu::MultisampleState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUMultisampleState>> {
  static std::shared_ptr<rnwgpu::GPUMultisampleState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUMultisampleState>();
    if (value.hasProperty(runtime, "count")) {
      auto count = value.getProperty(runtime, "count");
      if (count.isNumber()) {
        result->_instance.count = count.getNumber();
      } else if (count.isNull() || count.isUndefined()) {
        throw std::runtime_error(
            "Property GPUMultisampleState::count is required");
      }
    }
    if (value.hasProperty(runtime, "mask")) {
      auto mask = value.getProperty(runtime, "mask");
      if (mask.isNumber()) {
        result->_instance.mask = mask.getNumber();
      } else if (mask.isNull() || mask.isUndefined()) {
        throw std::runtime_error(
            "Property GPUMultisampleState::mask is required");
      }
    }
    if (value.hasProperty(runtime, "alphaToCoverageEnabled")) {
      auto alphaToCoverageEnabled =
          value.getProperty(runtime, "alphaToCoverageEnabled");
      if (alphaToCoverageEnabled.isNumber()) {
        result->_instance.alphaToCoverageEnabled =
            alphaToCoverageEnabled.getNumber();
      } else if (alphaToCoverageEnabled.isNull() ||
                 alphaToCoverageEnabled.isUndefined()) {
        throw std::runtime_error(
            "Property GPUMultisampleState::alphaToCoverageEnabled is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUMultisampleState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
