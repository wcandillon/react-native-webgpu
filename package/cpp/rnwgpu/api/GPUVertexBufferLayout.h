#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::VertexBufferLayout>> {
  static std::shared_ptr<wgpu::VertexBufferLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::VertexBufferLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "arrayStride")) {
        auto arrayStride = value.getProperty(runtime, "arrayStride");

        if (arrayStride.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexBufferLayout::arrayStride is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexBufferLayout::arrayStride is not defined");
      }
      if (value.hasProperty(runtime, "stepMode")) {
        auto stepMode = value.getProperty(runtime, "stepMode");
      }
      if (value.hasProperty(runtime, "attributes")) {
        auto attributes = value.getProperty(runtime, "attributes");

        if (attributes.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexBufferLayout::attributes is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexBufferLayout::attributes is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUVertexBufferLayout::arrayStride = %f",
                                 result->_instance.arrayStride);
    rnwgpu::Logger::logToConsole("GPUVertexBufferLayout::stepMode = %f",
                                 result->_instance.stepMode);
    rnwgpu::Logger::logToConsole("GPUVertexBufferLayout::attributes = %f",
                                 result->_instance.attributes);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::VertexBufferLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
