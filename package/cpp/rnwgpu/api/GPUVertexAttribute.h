#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::VertexAttribute>> {
  static std::shared_ptr<wgpu::VertexAttribute>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::VertexAttribute>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexAttribute::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexAttribute::format is not defined");
      }
      if (value.hasProperty(runtime, "offset")) {
        auto offset = value.getProperty(runtime, "offset");

        if (offset.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexAttribute::offset is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexAttribute::offset is not defined");
      }
      if (value.hasProperty(runtime, "shaderLocation")) {
        auto shaderLocation = value.getProperty(runtime, "shaderLocation");

        if (shaderLocation.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexAttribute::shaderLocation is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexAttribute::shaderLocation is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPUVertexAttribute::format = %f",
                                 result->format);
    rnwgpu::Logger::logToConsole("GPUVertexAttribute::offset = %f",
                                 result->offset);
    rnwgpu::Logger::logToConsole("GPUVertexAttribute::shaderLocation = %f",
                                 result->shaderLocation);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::VertexAttribute> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
