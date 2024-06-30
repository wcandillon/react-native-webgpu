#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUDeviceDescriptor {
public:
  wgpu::DeviceDescriptor *getInstance() { return &_instance; }

private:
  wgpu::DeviceDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDeviceDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUDeviceDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUDeviceDescriptor>();
    if (value.hasProperty(runtime, "requiredFeatures")) {
      auto requiredFeatures = value.getProperty(runtime, "requiredFeatures");
      if (requiredFeatures.isNumber()) {
        result->_instance.requiredFeatures = requiredFeatures.getNumber();
      } else if (requiredFeatures.isNull() || requiredFeatures.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDeviceDescriptor::requiredFeatures is required");
      }
    }
    if (value.hasProperty(runtime, "requiredLimits")) {
      auto requiredLimits = value.getProperty(runtime, "requiredLimits");
      if (requiredLimits.isNumber()) {
        result->_instance.requiredLimits = requiredLimits.getNumber();
      } else if (requiredLimits.isNull() || requiredLimits.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDeviceDescriptor::requiredLimits is required");
      }
    }
    if (value.hasProperty(runtime, "defaultQueue")) {
      auto defaultQueue = value.getProperty(runtime, "defaultQueue");
      if (defaultQueue.isNumber()) {
        result->_instance.defaultQueue = defaultQueue.getNumber();
      } else if (defaultQueue.isNull() || defaultQueue.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDeviceDescriptor::defaultQueue is required");
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
