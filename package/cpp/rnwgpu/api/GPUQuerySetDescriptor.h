#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUQuerySetDescriptor {
public:
  wgpu::QuerySetDescriptor *getInstance() { return &_instance; }

  wgpu::QuerySetDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUQuerySetDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUQuerySetDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUQuerySetDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "type")) {
        auto type = value.getProperty(runtime, "type");

        if (type.isUndefined()) {
          throw std::runtime_error(
              "Property GPUQuerySetDescriptor::type is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUQuerySetDescriptor::type is not defined");
      }
      if (value.hasProperty(runtime, "count")) {
        auto count = value.getProperty(runtime, "count");

        if (count.isNumber()) {
          result->_instance.count =
              static_cast<wgpu::Size32>(count.getNumber());
        }

        if (count.isUndefined()) {
          throw std::runtime_error(
              "Property GPUQuerySetDescriptor::count is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUQuerySetDescriptor::count is not defined");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");
      }
    }
    rnwgpu::Logger::logToConsole("GPUQuerySetDescriptor::type = %f",
                                 result->_instance.type);
    rnwgpu::Logger::logToConsole("GPUQuerySetDescriptor::count = %f",
                                 result->_instance.count);
    rnwgpu::Logger::logToConsole("GPUQuerySetDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUQuerySetDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
