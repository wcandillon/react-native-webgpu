#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUCanvasConfiguration {
public:
  wgpu::CanvasConfiguration *getInstance() { return &_instance; }

private:
  wgpu::CanvasConfiguration _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (value.hasProperty(runtime, "device")) {
      auto device = value.getProperty(runtime, "device");
      if (device.isNumber()) {
        result->_instance.device = device.getNumber();
      }
    }
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      }
    }
    if (value.hasProperty(runtime, "usage")) {
      auto usage = value.getProperty(runtime, "usage");
      if (usage.isNumber()) {
        result->_instance.usage = usage.getNumber();
      } else if (usage.isNull() || usage.isUndefined()) {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::usage is required");
      }
    }
    if (value.hasProperty(runtime, "viewFormats")) {
      auto viewFormats = value.getProperty(runtime, "viewFormats");
      if (viewFormats.isNumber()) {
        result->_instance.viewFormats = viewFormats.getNumber();
      } else if (viewFormats.isNull() || viewFormats.isUndefined()) {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::viewFormats is required");
      }
    }
    if (value.hasProperty(runtime, "colorSpace")) {
      auto colorSpace = value.getProperty(runtime, "colorSpace");
      if (colorSpace.isNumber()) {
        result->_instance.colorSpace = colorSpace.getNumber();
      } else if (colorSpace.isNull() || colorSpace.isUndefined()) {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::colorSpace is required");
      }
    }
    if (value.hasProperty(runtime, "alphaMode")) {
      auto alphaMode = value.getProperty(runtime, "alphaMode");
      if (alphaMode.isNumber()) {
        result->_instance.alphaMode = alphaMode.getNumber();
      } else if (alphaMode.isNull() || alphaMode.isUndefined()) {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::alphaMode is required");
      }
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
