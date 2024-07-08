#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

// TODO: Delete this class and use std::shared_ptr<wgpu::BufferDescriptor>
// instead
class GPUBufferDescriptor {
public:
  wgpu::BufferDescriptor *getInstance() { return &_instance; }

  wgpu::BufferDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBufferDescriptor>();
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
          result->_instance.label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::size = %f",
                                 result->_instance.size);
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::usage = %f",
                                 result->_instance.usage);
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::mappedAtCreation = %f",
                                 result->_instance.mappedAtCreation);
    rnwgpu::Logger::logToConsole("GPUBufferDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
