#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUPipelineLayoutDescriptor {
public:
  wgpu::PipelineLayoutDescriptor *getInstance() { return &_instance; }

  wgpu::PipelineLayoutDescriptor _instance;

  std::string label;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineLayoutDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUPipelineLayoutDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPipelineLayoutDescriptor>();
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
          result->label = str;
          result->_instance.label = result->label.c_str();
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
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUPipelineLayoutDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
