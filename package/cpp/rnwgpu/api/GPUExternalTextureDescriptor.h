#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::ExternalTextureDescriptor>> {
  static std::shared_ptr<wgpu::ExternalTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ExternalTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "source")) {
        auto source = value.getProperty(runtime, "source");

        if (source.isUndefined()) {
          throw std::runtime_error(
              "Property GPUExternalTextureDescriptor::source is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUExternalTextureDescriptor::source is not defined");
      }
      if (value.hasProperty(runtime, "colorSpace")) {
        auto colorSpace = value.getProperty(runtime, "colorSpace");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUExternalTextureDescriptor::source = %f",
                                 result->source);
    rnwgpu::Logger::logToConsole(
        "GPUExternalTextureDescriptor::colorSpace = %f", result->colorSpace);
    rnwgpu::Logger::logToConsole("GPUExternalTextureDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<wgpu::ExternalTextureDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
