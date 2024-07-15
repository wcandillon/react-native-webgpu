#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUTextureView.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

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

        if (view.isObject()) {
          auto val = m::JSIConverter<rnwgpu::GPUTextureView>::fromJSI(
              runtime, view, false);
          result->_instance.view = val._instance;
        }

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

        if (depthLoadOp.isString()) {
          auto str = depthLoadOp.asString(runtime).utf8(runtime);
          wgpu::LoadOp enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.depthLoadOp = enumValue;
        }
      }
      if (value.hasProperty(runtime, "depthStoreOp")) {
        auto depthStoreOp = value.getProperty(runtime, "depthStoreOp");

        if (depthStoreOp.isString()) {
          auto str = depthStoreOp.asString(runtime).utf8(runtime);
          wgpu::StoreOp enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.depthStoreOp = enumValue;
        }
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
              static_cast<uint32_t>(stencilClearValue.getNumber());
        }
      }
      if (value.hasProperty(runtime, "stencilLoadOp")) {
        auto stencilLoadOp = value.getProperty(runtime, "stencilLoadOp");

        if (stencilLoadOp.isString()) {
          auto str = stencilLoadOp.asString(runtime).utf8(runtime);
          wgpu::LoadOp enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.stencilLoadOp = enumValue;
        }
      }
      if (value.hasProperty(runtime, "stencilStoreOp")) {
        auto stencilStoreOp = value.getProperty(runtime, "stencilStoreOp");

        if (stencilStoreOp.isString()) {
          auto str = stencilStoreOp.asString(runtime).utf8(runtime);
          wgpu::StoreOp enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.stencilStoreOp = enumValue;
        }
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
        if (stencilReadOnly.isBool()) {
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
