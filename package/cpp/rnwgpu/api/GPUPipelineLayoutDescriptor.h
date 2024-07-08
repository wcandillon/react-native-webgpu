#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::PipelineLayoutDescriptor>> {
  static std::shared_ptr<wgpu::PipelineLayoutDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::PipelineLayoutDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "bindGroupLayouts")) {
        auto bindGroupLayouts = value.getProperty(runtime, "bindGroupLayouts");

        if (bindGroupLayouts.isUndefined()) {
          throw std::runtime_error(
              "Property GPUPipelineLayoutDescriptor::bindGroupLayouts is "
              "required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUPipelineLayoutDescriptor::bindGroupLayouts is not "
            "defined");
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
        "GPUPipelineLayoutDescriptor::bindGroupLayouts = %f",
        result->_instance.bindGroupLayouts);
    rnwgpu::Logger::logToConsole("GPUPipelineLayoutDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::PipelineLayoutDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
