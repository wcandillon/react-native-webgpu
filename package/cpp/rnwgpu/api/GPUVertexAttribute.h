#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUVertexAttribute {
public:
  wgpu::VertexAttribute *getInstance() { return &_instance; }

private:
  wgpu::VertexAttribute _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexAttribute>> {
  static std::shared_ptr<rnwgpu::GPUVertexAttribute>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUVertexAttribute>();
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      }
    }
    if (value.hasProperty(runtime, "offset")) {
      auto offset = value.getProperty(runtime, "offset");
      if (offset.isNumber()) {
        result->_instance.offset = offset.getNumber();
      }
    }
    if (value.hasProperty(runtime, "shaderLocation")) {
      auto shaderLocation = value.getProperty(runtime, "shaderLocation");
      if (shaderLocation.isNumber()) {
        result->_instance.shaderLocation = shaderLocation.getNumber();
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
