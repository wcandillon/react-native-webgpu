#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassDescriptor {
public:
  wgpu::RenderPassDescriptor *getInstance() { return &_instance; }

private:
  wgpu::RenderPassDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPassDescriptor>();
    if (value.hasProperty(runtime, "colorAttachments")) {
      auto colorAttachments = value.getProperty(runtime, "colorAttachments");
      if (colorAttachments.isNumber()) {
        result->_instance.colorAttachments = colorAttachments.getNumber();
      }
    }
    if (value.hasProperty(runtime, "depthStencilAttachment")) {
      auto depthStencilAttachment =
          value.getProperty(runtime, "depthStencilAttachment");
      if (depthStencilAttachment.isNumber()) {
        result->_instance.depthStencilAttachment =
            depthStencilAttachment.getNumber();
      } else if (depthStencilAttachment.isNull() ||
                 depthStencilAttachment.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDescriptor::depthStencilAttachment is "
            "required");
      }
    }
    if (value.hasProperty(runtime, "occlusionQuerySet")) {
      auto occlusionQuerySet = value.getProperty(runtime, "occlusionQuerySet");
      if (occlusionQuerySet.isNumber()) {
        result->_instance.occlusionQuerySet = occlusionQuerySet.getNumber();
      } else if (occlusionQuerySet.isNull() ||
                 occlusionQuerySet.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDescriptor::occlusionQuerySet is required");
      }
    }
    if (value.hasProperty(runtime, "timestampWrites")) {
      auto timestampWrites = value.getProperty(runtime, "timestampWrites");
      if (timestampWrites.isNumber()) {
        result->_instance.timestampWrites = timestampWrites.getNumber();
      } else if (timestampWrites.isNull() || timestampWrites.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDescriptor::timestampWrites is required");
      }
    }
    if (value.hasProperty(runtime, "maxDrawCount")) {
      auto maxDrawCount = value.getProperty(runtime, "maxDrawCount");
      if (maxDrawCount.isNumber()) {
        result->_instance.maxDrawCount = maxDrawCount.getNumber();
      } else if (maxDrawCount.isNull() || maxDrawCount.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassDescriptor::maxDrawCount is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
