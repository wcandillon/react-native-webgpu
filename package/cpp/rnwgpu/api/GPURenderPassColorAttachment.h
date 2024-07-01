#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassColorAttachment {
public:
  wgpu::RenderPassColorAttachment *getInstance() { return &_instance; }

  wgpu::RenderPassColorAttachment _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPassColorAttachment>();
    if (value.hasProperty(runtime, "view")) {
      auto view = value.getProperty(runtime, "view");

      else if (view.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::view is required");
      }
    }
    if (value.hasProperty(runtime, "depthSlice")) {
      auto depthSlice = value.getProperty(runtime, "depthSlice");

      if (value.hasProperty(runtime, "depthSlice")) {
        result->_instance.depthSlice = depthSlice.getNumber();
      }
    }
    if (value.hasProperty(runtime, "resolveTarget")) {
      auto resolveTarget = value.getProperty(runtime, "resolveTarget");
    }
    if (value.hasProperty(runtime, "clearValue")) {
      auto clearValue = value.getProperty(runtime, "clearValue");
    }
    if (value.hasProperty(runtime, "loadOp")) {
      auto loadOp = value.getProperty(runtime, "loadOp");

      else if (loadOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::loadOp is required");
      }
    }
    if (value.hasProperty(runtime, "storeOp")) {
      auto storeOp = value.getProperty(runtime, "storeOp");

      else if (storeOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::storeOp is required");
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
