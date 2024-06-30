#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPipelineDescriptor {
public:
  wgpu::RenderPipelineDescriptor *getInstance() { return &_instance; }

private:
  wgpu::RenderPipelineDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPipelineDescriptor>();
    if (value.hasProperty(runtime, "vertex")) {
      auto vertex = value.getProperty(runtime, "vertex");
      if (vertex.isNumber()) {
        result->_instance.vertex = vertex.getNumber();
      }
    }
    if (value.hasProperty(runtime, "primitive")) {
      auto primitive = value.getProperty(runtime, "primitive");
      if (primitive.isNumber()) {
        result->_instance.primitive = primitive.getNumber();
      } else if (primitive.isNull() || primitive.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPipelineDescriptor::primitive is required");
      }
    }
    if (value.hasProperty(runtime, "depthStencil")) {
      auto depthStencil = value.getProperty(runtime, "depthStencil");
      if (depthStencil.isNumber()) {
        result->_instance.depthStencil = depthStencil.getNumber();
      } else if (depthStencil.isNull() || depthStencil.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPipelineDescriptor::depthStencil is required");
      }
    }
    if (value.hasProperty(runtime, "multisample")) {
      auto multisample = value.getProperty(runtime, "multisample");
      if (multisample.isNumber()) {
        result->_instance.multisample = multisample.getNumber();
      } else if (multisample.isNull() || multisample.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPipelineDescriptor::multisample is required");
      }
    }
    if (value.hasProperty(runtime, "fragment")) {
      auto fragment = value.getProperty(runtime, "fragment");
      if (fragment.isNumber()) {
        result->_instance.fragment = fragment.getNumber();
      } else if (fragment.isNull() || fragment.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPipelineDescriptor::fragment is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
