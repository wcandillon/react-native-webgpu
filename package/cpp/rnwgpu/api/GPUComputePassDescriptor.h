#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUComputePassDescriptor {
public:
  wgpu::ComputePassDescriptor *getInstance() { return &_instance; }

private:
  wgpu::ComputePassDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUComputePassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUComputePassDescriptor>();
    if (value.hasProperty(runtime, "timestampWrites")) {
      auto timestampWrites = value.getProperty(runtime, "timestampWrites");
      if (timestampWrites.isNumber()) {
        result->_instance.timestampWrites = timestampWrites.getNumber();
      } else if (timestampWrites.isNull() || timestampWrites.isUndefined()) {
        throw std::runtime_error(
            "Property GPUComputePassDescriptor::timestampWrites is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUComputePassDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
