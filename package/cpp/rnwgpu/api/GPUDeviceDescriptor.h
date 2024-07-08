#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::DeviceDescriptor>> {
  static std::shared_ptr<wgpu::DeviceDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::DeviceDescriptor>();
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
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUDeviceDescriptor::requiredFeatures = %f",
                                 result->requiredFeatures);
    rnwgpu::Logger::logToConsole("GPUDeviceDescriptor::requiredLimits = %f",
                                 result->requiredLimits);
    rnwgpu::Logger::logToConsole("GPUDeviceDescriptor::defaultQueue = %f",
                                 result->defaultQueue);
    rnwgpu::Logger::logToConsole("GPUDeviceDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::DeviceDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
