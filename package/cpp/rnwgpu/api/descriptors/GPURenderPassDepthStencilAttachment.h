#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

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
      } else {
        throw std::runtime_error(
            "Property GPURenderPassDepthStencilAttachment::view is not "
            "defined");
      }
      if (value.hasProperty(runtime, "depthClearValue")) {
        auto depthClearValue = value.getProperty(runtime, "depthClearValue");

        if (depthClearValue.isNumber()) {
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
        if (depthReadOnly.isBool()) {
          result->_instance.depthReadOnly = depthReadOnly.getBool();
        }
      }
      if (value.hasProperty(runtime, "stencilClearValue")) {
        auto stencilClearValue =
            value.getProperty(runtime, "stencilClearValue");

        if (stencilClearValue.isNumber()) {
          result->_instance.stencilClearValue =
              static_cast<wgpu::StencilValue>(stencilClearValue.getNumber());
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
        if (stencilReadOnly.isBool()) {
          result->_instance.stencilReadOnly = stencilReadOnly.getBool();
        }
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::view = %f",
        result->_instance.view);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::depthClearValue = %f",
        result->_instance.depthClearValue);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::depthLoadOp = %f",
        result->_instance.depthLoadOp);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::depthStoreOp = %f",
        result->_instance.depthStoreOp);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::depthReadOnly = %f",
        result->_instance.depthReadOnly);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::stencilClearValue = %f",
        result->_instance.stencilClearValue);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::stencilLoadOp = %f",
        result->_instance.stencilLoadOp);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::stencilStoreOp = %f",
        result->_instance.stencilStoreOp);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDepthStencilAttachment::stencilReadOnly = %f",
        result->_instance.stencilReadOnly);
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
