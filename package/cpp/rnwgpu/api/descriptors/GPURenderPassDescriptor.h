#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUQuerySet.h"
#include "GPURenderPassDepthStencilAttachment.h"
#include "GPURenderPassTimestampWrites.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPURenderPassDescriptor {
public:
  wgpu::RenderPassDescriptor *getInstance() { return &_instance; }

  wgpu::RenderPassDescriptor _instance;

  std::string label;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "colorAttachments")) {
        auto colorAttachments = value.getProperty(runtime, "colorAttachments");

        if (colorAttachments.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassDescriptor::colorAttachments is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassDescriptor::colorAttachments is not "
            "defined");
      }
      if (value.hasProperty(runtime, "depthStencilAttachment")) {
        auto depthStencilAttachment =
            value.getProperty(runtime, "depthStencilAttachment");

        if (depthStencilAttachment.isObject()) {
          auto val = m::JSIConverter<
              std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>>::
              fromJSI(runtime, depthStencilAttachment, false);
          result->_instance.depthStencilAttachment = val->getInstance();
        }
      }
      if (value.hasProperty(runtime, "occlusionQuerySet")) {
        auto occlusionQuerySet =
            value.getProperty(runtime, "occlusionQuerySet");

        if (occlusionQuerySet.isObject() &&
            occlusionQuerySet.getObject(runtime).isHostObject(runtime)) {
          result->_instance.occlusionQuerySet =
              occlusionQuerySet.getObject(runtime)
                  .asHostObject<rnwgpu::GPUQuerySet>(runtime)
                  ->get();
        }
      }
      if (value.hasProperty(runtime, "timestampWrites")) {
        auto timestampWrites = value.getProperty(runtime, "timestampWrites");

        if (timestampWrites.isObject()) {
          auto val = m::JSIConverter<std::shared_ptr<
              rnwgpu::GPURenderPassTimestampWrites>>::fromJSI(runtime,
                                                              timestampWrites,
                                                              false);
          result->_instance.timestampWrites = val->getInstance();
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str;
          result->_instance.label = result->label.c_str();
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
