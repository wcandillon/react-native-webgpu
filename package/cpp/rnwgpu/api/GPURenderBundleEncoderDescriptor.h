#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderBundleEncoderDescriptor {
public:
  wgpu::RenderBundleEncoderDescriptor *getInstance() { return &_instance; }

private:
  wgpu::RenderBundleEncoderDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderBundleEncoderDescriptor>();
    if (value.hasProperty(runtime, "depthReadOnly")) {
      auto depthReadOnly = value.getProperty(runtime, "depthReadOnly");
      if (depthReadOnly.isNumber()) {
        result->_instance.depthReadOnly = depthReadOnly.getNumber();
      } else if (depthReadOnly.isNull() || depthReadOnly.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderBundleEncoderDescriptor::depthReadOnly is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "stencilReadOnly")) {
      auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
      if (stencilReadOnly.isNumber()) {
        result->_instance.stencilReadOnly = stencilReadOnly.getNumber();
      } else if (stencilReadOnly.isNull() || stencilReadOnly.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderBundleEncoderDescriptor::stencilReadOnly is "
            "required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
