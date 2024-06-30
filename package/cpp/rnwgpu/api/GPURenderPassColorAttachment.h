#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassColorAttachment {
public:
  wgpu::RenderPassColorAttachment *getInstance() { return &_instance; }

private:
  wgpu::RenderPassColorAttachment _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPassColorAttachment>();
    if (value.hasProperty(runtime, "view")) {
      auto view = value.getProperty(runtime, "view");
      if (view.isNumber()) {
        result->_instance.view = view.getNumber();
      }
    }
    if (value.hasProperty(runtime, "depthSlice")) {
      auto depthSlice = value.getProperty(runtime, "depthSlice");
      if (depthSlice.isNumber()) {
        result->_instance.depthSlice = depthSlice.getNumber();
      } else if (depthSlice.isNull() || depthSlice.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::depthSlice is required");
      }
    }
    if (value.hasProperty(runtime, "resolveTarget")) {
      auto resolveTarget = value.getProperty(runtime, "resolveTarget");
      if (resolveTarget.isNumber()) {
        result->_instance.resolveTarget = resolveTarget.getNumber();
      } else if (resolveTarget.isNull() || resolveTarget.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::resolveTarget is required");
      }
    }
    if (value.hasProperty(runtime, "clearValue")) {
      auto clearValue = value.getProperty(runtime, "clearValue");
      if (clearValue.isNumber()) {
        result->_instance.clearValue = clearValue.getNumber();
      } else if (clearValue.isNull() || clearValue.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::clearValue is required");
      }
    }
    if (value.hasProperty(runtime, "loadOp")) {
      auto loadOp = value.getProperty(runtime, "loadOp");
      if (loadOp.isNumber()) {
        result->_instance.loadOp = loadOp.getNumber();
      }
    }
    if (value.hasProperty(runtime, "storeOp")) {
      auto storeOp = value.getProperty(runtime, "storeOp");
      if (storeOp.isNumber()) {
        result->_instance.storeOp = storeOp.getNumber();
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassColorAttachment> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
