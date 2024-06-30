#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURequestAdapterOptions {
public:
  wgpu::RequestAdapterOptions *getInstance() { return &_instance; }

private:
  wgpu::RequestAdapterOptions _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURequestAdapterOptions>> {
  static std::shared_ptr<rnwgpu::GPURequestAdapterOptions>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURequestAdapterOptions>();
    if (value.hasProperty(runtime, "powerPreference")) {
      auto powerPreference = value.getProperty(runtime, "powerPreference");
      if (powerPreference.isNumber()) {
        result->_instance.powerPreference = powerPreference.getNumber();
      } else if (powerPreference.isNull() || powerPreference.isUndefined()) {
        throw std::runtime_error(
            "Property GPURequestAdapterOptions::powerPreference is required");
      }
    }
    if (value.hasProperty(runtime, "forceFallbackAdapter")) {
      auto forceFallbackAdapter =
          value.getProperty(runtime, "forceFallbackAdapter");
      if (forceFallbackAdapter.isNumber()) {
        result->_instance.forceFallbackAdapter =
            forceFallbackAdapter.getNumber();
      } else if (forceFallbackAdapter.isNull() ||
                 forceFallbackAdapter.isUndefined()) {
        throw std::runtime_error(
            "Property GPURequestAdapterOptions::forceFallbackAdapter is "
            "required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURequestAdapterOptions> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
