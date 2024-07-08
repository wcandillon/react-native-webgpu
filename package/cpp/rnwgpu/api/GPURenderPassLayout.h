#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::RenderPassLayout>> {
  static std::shared_ptr<wgpu::RenderPassLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::RenderPassLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "colorFormats")) {
        auto colorFormats = value.getProperty(runtime, "colorFormats");

        if (colorFormats.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassLayout::colorFormats is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassLayout::colorFormats is not defined");
      }
      if (value.hasProperty(runtime, "depthStencilFormat")) {
        auto depthStencilFormat =
            value.getProperty(runtime, "depthStencilFormat");
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto sampleCount = value.getProperty(runtime, "sampleCount");

        if (sampleCount.isNumber()) {
          result->sampleCount =
              static_cast<wgpu::Size32>(sampleCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::colorFormats = %f",
                                 result->colorFormats);
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::depthStencilFormat = %f",
                                 result->depthStencilFormat);
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::sampleCount = %f",
                                 result->sampleCount);
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::RenderPassLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
