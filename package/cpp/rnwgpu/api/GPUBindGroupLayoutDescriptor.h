#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBindGroupLayoutDescriptor {
public:
  wgpu::BindGroupLayoutDescriptor *getInstance() { return &_instance; }

  wgpu::BindGroupLayoutDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupLayoutDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupLayoutDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupLayoutDescriptor>();
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
      }
    }
    rnwgpu::Logger::logToConsole("GPUBindGroupLayoutDescriptor::entries = %f",
                                 result->_instance.entries);
    rnwgpu::Logger::logToConsole("GPUBindGroupLayoutDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUBindGroupLayoutDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
