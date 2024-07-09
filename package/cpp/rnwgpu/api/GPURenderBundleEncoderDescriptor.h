#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPURenderBundleEncoderDescriptor {
public:
  wgpu::RenderBundleEncoderDescriptor *getInstance() { return &_instance; }

  wgpu::RenderBundleEncoderDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderBundleEncoderDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depthReadOnly")) {
        auto depthReadOnly = value.getProperty(runtime, "depthReadOnly");
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
      }
      if (value.hasProperty(runtime, "colorFormats")) {
        auto colorFormats = value.getProperty(runtime, "colorFormats");

        if (colorFormats.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderBundleEncoderDescriptor::colorFormats is "
              "required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderBundleEncoderDescriptor::colorFormats is not "
            "defined");
      }
      if (value.hasProperty(runtime, "depthStencilFormat")) {
        auto depthStencilFormat =
            value.getProperty(runtime, "depthStencilFormat");
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto sampleCount = value.getProperty(runtime, "sampleCount");

        if (sampleCount.isNumber()) {
          result->_instance.sampleCount =
              static_cast<wgpu::Size32>(sampleCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->_instance.label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::depthReadOnly = %f",
        result->_instance.depthReadOnly);
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::stencilReadOnly = %f",
        result->_instance.stencilReadOnly);
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::colorFormats = %f",
        result->_instance.colorFormats);
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::depthStencilFormat = %f",
        result->_instance.depthStencilFormat);
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::sampleCount = %f",
        result->_instance.sampleCount);
    rnwgpu::Logger::logToConsole("GPURenderBundleEncoderDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
