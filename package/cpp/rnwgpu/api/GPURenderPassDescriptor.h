#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassDescriptor {
public:
  wgpu::RenderPassDescriptor *getInstance() { return &_instance; }

  wgpu::RenderPassDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPURenderPassDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "colorAttachments")) {
        auto colorAttachments = value.getProperty(runtime, "colorAttachments");

        else if (colorAttachments.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassDescriptor::colorAttachments is required");
        }
      }
      if (value.hasProperty(runtime, "depthStencilAttachment")) {
        auto depthStencilAttachment =
            value.getProperty(runtime, "depthStencilAttachment");
      }
      if (value.hasProperty(runtime, "occlusionQuerySet")) {
        auto occlusionQuerySet =
            value.getProperty(runtime, "occlusionQuerySet");
      }
      if (value.hasProperty(runtime, "timestampWrites")) {
        auto timestampWrites = value.getProperty(runtime, "timestampWrites");
      }
      if (value.hasProperty(runtime, "maxDrawCount")) {
        auto maxDrawCount = value.getProperty(runtime, "maxDrawCount");

        if (value.hasProperty(runtime, "maxDrawCount")) {
          result->_instance.maxDrawCount = maxDrawCount.getNumber();
        }
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
