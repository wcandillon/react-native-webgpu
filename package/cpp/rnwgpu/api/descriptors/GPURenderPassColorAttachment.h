#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUColor.h"
#include "GPUTextureView.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

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

        if (view.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUTextureView>>::fromJSI(
                  runtime, view, false);
          result->_instance.view = val->_instance;
        }

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

        if (resolveTarget.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUTextureView>>::fromJSI(
                  runtime, resolveTarget, false);
          result->_instance.resolveTarget = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "clearValue")) {
        auto clearValue = value.getProperty(runtime, "clearValue");

        if (clearValue.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUColor>>::fromJSI(
                  runtime, clearValue, false);
          result->_instance.clearValue = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "loadOp")) {
        auto loadOp = value.getProperty(runtime, "loadOp");

        if (loadOp.isString()) {
          auto str = loadOp.asString(runtime).utf8(runtime);
          wgpu::LoadOp enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.loadOp = enumValue;
        }

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

        if (storeOp.isString()) {
          auto str = storeOp.asString(runtime).utf8(runtime);
          wgpu::StoreOp enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.storeOp = enumValue;
        }

        if (storeOp.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassColorAttachment::storeOp is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassColorAttachment::storeOp is not defined");
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
