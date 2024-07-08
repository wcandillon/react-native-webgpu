#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBindGroupEntry {
public:
  wgpu::BindGroupEntry *getInstance() { return &_instance; }

  wgpu::BindGroupEntry _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupEntry>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "binding")) {
        auto binding = value.getProperty(runtime, "binding");

        if (binding.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupEntry::binding is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupEntry::binding is not defined");
      }
      if (value.hasProperty(runtime, "resource")) {
        auto resource = value.getProperty(runtime, "resource");

        if (resource.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupEntry::resource is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupEntry::resource is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUBindGroupEntry::binding = %f",
                                 result->_instance.binding);
    rnwgpu::Logger::logToConsole("GPUBindGroupEntry::resource = %f",
                                 result->_instance.resource);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupEntry> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
