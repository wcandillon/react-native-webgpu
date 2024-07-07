#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUDeviceDescriptor {
public:
  wgpu::DeviceDescriptor *getInstance() { return &_instance; }

  wgpu::DeviceDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDeviceDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUDeviceDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDeviceDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "requiredFeatures")) {
        auto requiredFeatures = value.getProperty(runtime, "requiredFeatures");
      }
      if (value.hasProperty(runtime, "requiredLimits")) {
        auto requiredLimits = value.getProperty(runtime, "requiredLimits");
      }
      if (value.hasProperty(runtime, "defaultQueue")) {
        auto defaultQueue = value.getProperty(runtime, "defaultQueue");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDeviceDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
