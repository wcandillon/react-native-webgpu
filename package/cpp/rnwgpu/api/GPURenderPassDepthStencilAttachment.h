#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassDepthStencilAttachment {
public:
  wgpu::RenderPassDepthStencilAttachment *getInstance() { return &_instance; }

  wgpu::RenderPassDepthStencilAttachment _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<
    std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result =
        std::make_unique<rnwgpu::GPURenderPassDepthStencilAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "view")) {
        auto view = value.getProperty(runtime, "view");

        if (view.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassDepthStencilAttachment::view is required");
        }
      }
      if (value.hasProperty(runtime, "depthClearValue")) {
        auto depthClearValue = value.getProperty(runtime, "depthClearValue");

        if (value.hasProperty(runtime, "depthClearValue")) {
          result->_instance.depthClearValue = depthClearValue.getNumber();
        }
      }
      if (value.hasProperty(runtime, "depthLoadOp")) {
        auto depthLoadOp = value.getProperty(runtime, "depthLoadOp");
      }
      if (value.hasProperty(runtime, "depthStoreOp")) {
        auto depthStoreOp = value.getProperty(runtime, "depthStoreOp");
      }
      if (value.hasProperty(runtime, "depthReadOnly")) {
        auto depthReadOnly = value.getProperty(runtime, "depthReadOnly");
        if (value.hasProperty(runtime, "depthReadOnly")) {
          result->_instance.depthReadOnly = depthReadOnly.getBool();
        }
      }
      if (value.hasProperty(runtime, "stencilClearValue")) {
        auto stencilClearValue =
            value.getProperty(runtime, "stencilClearValue");

        if (value.hasProperty(runtime, "stencilClearValue")) {
          result->_instance.stencilClearValue = stencilClearValue.getNumber();
        }
      }
      if (value.hasProperty(runtime, "stencilLoadOp")) {
        auto stencilLoadOp = value.getProperty(runtime, "stencilLoadOp");
      }
      if (value.hasProperty(runtime, "stencilStoreOp")) {
        auto stencilStoreOp = value.getProperty(runtime, "stencilStoreOp");
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
        if (value.hasProperty(runtime, "stencilReadOnly")) {
          result->_instance.stencilReadOnly = stencilReadOnly.getBool();
        }
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
