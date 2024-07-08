#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

// TODO: Delete this class and use
// std::shared_ptr<wgpu::RenderPassColorAttachment> instead
class GPURenderPassColorAttachment {
public:
  wgpu::RenderPassColorAttachment *getInstance() { return &_instance; }

  wgpu::RenderPassColorAttachment _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassColorAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "view")) {
        auto view = value.getProperty(runtime, "view");

        if (view.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassColorAttachment::view is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::view is not defined");
      }
      if (value.hasProperty(runtime, "depthSlice")) {
        auto depthSlice = value.getProperty(runtime, "depthSlice");

        if (depthSlice.isNumber()) {
          result->_instance.depthSlice =
              static_cast<wgpu::IntegerCoordinate>(depthSlice.getNumber());
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

        if (loadOp.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassColorAttachment::loadOp is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::loadOp is not defined");
      }
      if (value.hasProperty(runtime, "storeOp")) {
        auto storeOp = value.getProperty(runtime, "storeOp");

        if (storeOp.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassColorAttachment::storeOp is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::storeOp is not defined");
      }
    }
    rnwgpu::Logger::logToConsole("GPURenderPassColorAttachment::view = %f",
                                 result->_instance.view);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassColorAttachment::depthSlice = %f",
        result->_instance.depthSlice);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassColorAttachment::resolveTarget = %f",
        result->_instance.resolveTarget);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassColorAttachment::clearValue = %f",
        result->_instance.clearValue);
    rnwgpu::Logger::logToConsole("GPURenderPassColorAttachment::loadOp = %f",
                                 result->_instance.loadOp);
    rnwgpu::Logger::logToConsole("GPURenderPassColorAttachment::storeOp = %f",
                                 result->_instance.storeOp);
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
