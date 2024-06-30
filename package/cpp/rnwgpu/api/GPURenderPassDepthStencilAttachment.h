#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassDepthStencilAttachment {
public:
  wgpu::RenderPassDepthStencilAttachment *getInstance() { return &_instance; }

private:
  wgpu::RenderPassDepthStencilAttachment _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<
    std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result =
        std::make_unique<rnwgpu::GPURenderPassDepthStencilAttachment>();
    if (value.hasProperty(runtime, "view")) {
      auto view = value.getProperty(runtime, "view");
      if (view.isNumber()) {
        result->_instance.view = view.getNumber();
      }
    }
    if (value.hasProperty(runtime, "depthClearValue")) {
      auto depthClearValue = value.getProperty(runtime, "depthClearValue");
      if (depthClearValue.isNumber()) {
        result->_instance.depthClearValue = depthClearValue.getNumber();
      } else if (depthClearValue.isNull() || depthClearValue.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::depthClearValue is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "depthLoadOp")) {
      auto depthLoadOp = value.getProperty(runtime, "depthLoadOp");
      if (depthLoadOp.isNumber()) {
        result->_instance.depthLoadOp = depthLoadOp.getNumber();
      } else if (depthLoadOp.isNull() || depthLoadOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::depthLoadOp is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "depthStoreOp")) {
      auto depthStoreOp = value.getProperty(runtime, "depthStoreOp");
      if (depthStoreOp.isNumber()) {
        result->_instance.depthStoreOp = depthStoreOp.getNumber();
      } else if (depthStoreOp.isNull() || depthStoreOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::depthStoreOp is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "depthReadOnly")) {
      auto depthReadOnly = value.getProperty(runtime, "depthReadOnly");
      if (depthReadOnly.isNumber()) {
        result->_instance.depthReadOnly = depthReadOnly.getNumber();
      } else if (depthReadOnly.isNull() || depthReadOnly.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::depthReadOnly is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "stencilClearValue")) {
      auto stencilClearValue = value.getProperty(runtime, "stencilClearValue");
      if (stencilClearValue.isNumber()) {
        result->_instance.stencilClearValue = stencilClearValue.getNumber();
      } else if (stencilClearValue.isNull() ||
                 stencilClearValue.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::stencilClearValue "
            "is required");
      }
    }
    if (value.hasProperty(runtime, "stencilLoadOp")) {
      auto stencilLoadOp = value.getProperty(runtime, "stencilLoadOp");
      if (stencilLoadOp.isNumber()) {
        result->_instance.stencilLoadOp = stencilLoadOp.getNumber();
      } else if (stencilLoadOp.isNull() || stencilLoadOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::stencilLoadOp is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "stencilStoreOp")) {
      auto stencilStoreOp = value.getProperty(runtime, "stencilStoreOp");
      if (stencilStoreOp.isNumber()) {
        result->_instance.stencilStoreOp = stencilStoreOp.getNumber();
      } else if (stencilStoreOp.isNull() || stencilStoreOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::stencilStoreOp is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "stencilReadOnly")) {
      auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
      if (stencilReadOnly.isNumber()) {
        result->_instance.stencilReadOnly = stencilReadOnly.getNumber();
      } else if (stencilReadOnly.isNull() || stencilReadOnly.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::stencilReadOnly is "
            "required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
