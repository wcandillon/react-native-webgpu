#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::BindGroupLayoutDescriptor>> {
  static std::shared_ptr<wgpu::BindGroupLayoutDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::BindGroupLayoutDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "entries")) {
        auto entries = value.getProperty(runtime, "entries");

        if (entries.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupLayoutDescriptor::entries is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutDescriptor::entries is not defined");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUBindGroupLayoutDescriptor::entries = %f",
                                 result->entries);
    rnwgpu::Logger::logToConsole("GPUBindGroupLayoutDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<wgpu::BindGroupLayoutDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
