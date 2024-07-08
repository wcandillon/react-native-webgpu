#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::BufferDescriptor>> {
  static std::shared_ptr<wgpu::BufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::BufferDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "size")) {
        auto size = value.getProperty(runtime, "size");

        if (size.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBufferDescriptor::size is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBufferDescriptor::size is not defined");
      }
      if (value.hasProperty(runtime, "usage")) {
        auto usage = value.getProperty(runtime, "usage");

        if (usage.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBufferDescriptor::usage is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBufferDescriptor::usage is not defined");
      }
      if (value.hasProperty(runtime, "mappedAtCreation")) {
        auto mappedAtCreation = value.getProperty(runtime, "mappedAtCreation");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::size = %f",
                                 result->size);
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::usage = %f",
                                 result->usage);
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::mappedAtCreation = %f",
                                 result->mappedAtCreation);
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::BufferDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
