#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::StorageTextureBindingLayout>> {
  static std::shared_ptr<wgpu::StorageTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::StorageTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "access")) {
        auto access = value.getProperty(runtime, "access");
      }
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUStorageTextureBindingLayout::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUStorageTextureBindingLayout::format is not defined");
      }
      if (value.hasProperty(runtime, "viewDimension")) {
        auto viewDimension = value.getProperty(runtime, "viewDimension");
      }
    }
    rnwgpu::Logger::logToConsole("GPUStorageTextureBindingLayout::access = %f",
                                 result->_instance.access);
    rnwgpu::Logger::logToConsole("GPUStorageTextureBindingLayout::format = %f",
                                 result->_instance.format);
    rnwgpu::Logger::logToConsole(
        "GPUStorageTextureBindingLayout::viewDimension = %f",
        result->_instance.viewDimension);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<wgpu::StorageTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
