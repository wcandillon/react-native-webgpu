#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUComputePassDescriptor {
public:
  wgpu::ComputePassDescriptor *getInstance() { return &_instance; }

  wgpu::ComputePassDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUComputePassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUComputePassDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "timestampWrites")) {
        auto timestampWrites = value.getProperty(runtime, "timestampWrites");
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
