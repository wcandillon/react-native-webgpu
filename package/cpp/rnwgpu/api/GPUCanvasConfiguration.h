#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUCanvasConfiguration {
public:
  wgpu::CanvasConfiguration *getInstance() { return &_instance; }

  wgpu::CanvasConfiguration _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (value.hasProperty(runtime, "device")) {
      auto device = value.getProperty(runtime, "device");

      else if (device.isUndefined()) {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::device is required");
      }
    }
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");

      else if (format.isUndefined()) {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::format is required");
      }
    }
    if (value.hasProperty(runtime, "usage")) {
      auto usage = value.getProperty(runtime, "usage");

      if (value.hasProperty(runtime, "usage")) {
        result->_instance.usage = usage.getNumber();
      }
    }
    if (value.hasProperty(runtime, "viewFormats")) {
      auto viewFormats = value.getProperty(runtime, "viewFormats");
    }
    if (value.hasProperty(runtime, "colorSpace")) {
      auto colorSpace = value.getProperty(runtime, "colorSpace");
    }
    if (value.hasProperty(runtime, "alphaMode")) {
      auto alphaMode = value.getProperty(runtime, "alphaMode");
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUCanvasConfiguration> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
