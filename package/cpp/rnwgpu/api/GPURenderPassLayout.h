#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPURenderPassLayout {
public:
  wgpu::RenderPassLayout *getInstance() { return &_instance; }

  wgpu::RenderPassLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassLayout>> {
  static std::shared_ptr<rnwgpu::GPURenderPassLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassLayout>();
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
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::colorFormats = %f",
                                 result->_instance.colorFormats);
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::depthStencilFormat = %f",
                                 result->_instance.depthStencilFormat);
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::sampleCount = %f",
                                 result->_instance.sampleCount);
    rnwgpu::Logger::logToConsole("GPURenderPassLayout::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPURenderPassLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
