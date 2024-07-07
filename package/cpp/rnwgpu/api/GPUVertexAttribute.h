#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

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
      }
      if (value.hasProperty(runtime, "offset")) {
        auto offset = value.getProperty(runtime, "offset");

        if (offset.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexAttribute::offset is required");
        }
      }
      if (value.hasProperty(runtime, "shaderLocation")) {
        auto shaderLocation = value.getProperty(runtime, "shaderLocation");

        if (shaderLocation.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexAttribute::shaderLocation is required");
        }
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexAttribute> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
