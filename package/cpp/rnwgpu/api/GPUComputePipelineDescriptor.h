#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::ComputePipelineDescriptor>> {
  static std::shared_ptr<wgpu::ComputePipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ComputePipelineDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "compute")) {
        auto compute = value.getProperty(runtime, "compute");

        if (compute.isUndefined()) {
          throw std::runtime_error(
              "Property GPUComputePipelineDescriptor::compute is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUComputePipelineDescriptor::compute is not defined");
      }
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        if (layout.isUndefined()) {
          throw std::runtime_error(
              "Property GPUComputePipelineDescriptor::layout is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUComputePipelineDescriptor::layout is not defined");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->_instance.label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUComputePipelineDescriptor::compute = %f",
                                 result->_instance.compute);
    rnwgpu::Logger::logToConsole("GPUComputePipelineDescriptor::layout = %f",
                                 result->_instance.layout);
    rnwgpu::Logger::logToConsole("GPUComputePipelineDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<wgpu::ComputePipelineDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
