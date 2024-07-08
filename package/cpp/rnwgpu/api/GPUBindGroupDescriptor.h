#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::BindGroupDescriptor>> {
  static std::shared_ptr<wgpu::BindGroupDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::BindGroupDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        if (layout.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupDescriptor::layout is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupDescriptor::layout is not defined");
      }
      if (value.hasProperty(runtime, "entries")) {
        auto entries = value.getProperty(runtime, "entries");

        if (entries.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupDescriptor::entries is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupDescriptor::entries is not defined");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUBindGroupDescriptor::layout = %f",
                                 result->layout);
    rnwgpu::Logger::logToConsole("GPUBindGroupDescriptor::entries = %f",
                                 result->entries);
    rnwgpu::Logger::logToConsole("GPUBindGroupDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::BindGroupDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
