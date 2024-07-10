#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUVertexAttribute {
public:
  wgpu::VertexAttribute *getInstance() { return &_instance; }

  wgpu::VertexAttribute _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexAttribute>> {
  static std::shared_ptr<rnwgpu::GPUVertexAttribute>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUVertexAttribute>();
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

        if (offset.isNumber()) {
          result->_instance.offset = offset.getNumber();
        }

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

        if (shaderLocation.isNumber()) {
          result->_instance.shaderLocation =
              static_cast<wgpu::Index32>(shaderLocation.getNumber());
        }

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
                                 result->_instance.format);
    rnwgpu::Logger::logToConsole("GPUVertexAttribute::offset = %f",
                                 result->_instance.offset);
    rnwgpu::Logger::logToConsole("GPUVertexAttribute::shaderLocation = %f",
                                 result->_instance.shaderLocation);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexAttribute> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
