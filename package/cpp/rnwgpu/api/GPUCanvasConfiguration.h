#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUCanvasConfiguration {
public:
  wgpu::CanvasConfiguration *getInstance() { return &_instance; }

  wgpu::CanvasConfiguration _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "device")) {
        auto device = value.getProperty(runtime, "device");

        if (device.isUndefined()) {
          throw std::runtime_error(
              "Property GPUCanvasConfiguration::device is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::device is not defined");
      }
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUCanvasConfiguration::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::format is not defined");
      }
      if (value.hasProperty(runtime, "usage")) {
        auto usage = value.getProperty(runtime, "usage");
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
    }
    rnwgpu::Logger::logToConsole("GPUCanvasConfiguration::device = %f",
                                 result->_instance.device);
    rnwgpu::Logger::logToConsole("GPUCanvasConfiguration::format = %f",
                                 result->_instance.format);
    rnwgpu::Logger::logToConsole("GPUCanvasConfiguration::usage = %f",
                                 result->_instance.usage);
    rnwgpu::Logger::logToConsole("GPUCanvasConfiguration::viewFormats = %f",
                                 result->_instance.viewFormats);
    rnwgpu::Logger::logToConsole("GPUCanvasConfiguration::colorSpace = %f",
                                 result->_instance.colorSpace);
    rnwgpu::Logger::logToConsole("GPUCanvasConfiguration::alphaMode = %f",
                                 result->_instance.alphaMode);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUCanvasConfiguration> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
