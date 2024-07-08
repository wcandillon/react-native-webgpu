#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::RenderPassDescriptor>> {
  static std::shared_ptr<wgpu::RenderPassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::RenderPassDescriptor>();
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

        if (maxDrawCount.isNumber()) {
          result->maxDrawCount = maxDrawCount.getNumber();
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDescriptor::colorAttachments = %f",
        result->colorAttachments);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDescriptor::depthStencilAttachment = %f",
        result->depthStencilAttachment);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDescriptor::occlusionQuerySet = %f",
        result->occlusionQuerySet);
    rnwgpu::Logger::logToConsole(
        "GPURenderPassDescriptor::timestampWrites = %f",
        result->timestampWrites);
    rnwgpu::Logger::logToConsole("GPURenderPassDescriptor::maxDrawCount = %f",
                                 result->maxDrawCount);
    rnwgpu::Logger::logToConsole("GPURenderPassDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::RenderPassDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
