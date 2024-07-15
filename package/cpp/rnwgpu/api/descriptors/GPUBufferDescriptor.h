#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUBufferDescriptor {
public:
  wgpu::BufferDescriptor *getInstance() { return &_instance; }

  wgpu::BufferDescriptor _instance;

  std::string label;
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

        if (size.isNumber()) {
          result->_instance.size = size.getNumber();
        }

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

        if (usage.isNumber()) {
          result->_instance.usage =
              static_cast<wgpu::BufferUsage>(usage.getNumber());
        }

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
        if (mappedAtCreation.isBool()) {
          result->_instance.mappedAtCreation = mappedAtCreation.getBool();
        }
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

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
