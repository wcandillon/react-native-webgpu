#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPURenderPipelineDescriptor {
public:
  wgpu::RenderPipelineDescriptor *getInstance() { return &_instance; }

  wgpu::RenderPipelineDescriptor _instance;

  std::string label;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPipelineDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "vertex")) {
        auto vertex = value.getProperty(runtime, "vertex");

        if (vertex.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPipelineDescriptor::vertex is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPipelineDescriptor::vertex is not defined");
      }
      if (value.hasProperty(runtime, "primitive")) {
        auto primitive = value.getProperty(runtime, "primitive");
      }
      if (value.hasProperty(runtime, "depthStencil")) {
        auto depthStencil = value.getProperty(runtime, "depthStencil");
      }
      if (value.hasProperty(runtime, "multisample")) {
        auto multisample = value.getProperty(runtime, "multisample");
      }
      if (value.hasProperty(runtime, "fragment")) {
        auto fragment = value.getProperty(runtime, "fragment");
      }
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        if (layout.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPipelineDescriptor::layout is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPipelineDescriptor::layout is not defined");
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
    rnwgpu::Logger::logToConsole("GPURenderPipelineDescriptor::vertex = %f",
                                 result->_instance.vertex);
    rnwgpu::Logger::logToConsole("GPURenderPipelineDescriptor::primitive = %f",
                                 result->_instance.primitive);
    rnwgpu::Logger::logToConsole(
        "GPURenderPipelineDescriptor::depthStencil = %f",
        result->_instance.depthStencil);
    rnwgpu::Logger::logToConsole(
        "GPURenderPipelineDescriptor::multisample = %f",
        result->_instance.multisample);
    rnwgpu::Logger::logToConsole("GPURenderPipelineDescriptor::fragment = %f",
                                 result->_instance.fragment);
    rnwgpu::Logger::logToConsole("GPURenderPipelineDescriptor::layout = %f",
                                 result->_instance.layout);
    rnwgpu::Logger::logToConsole("GPURenderPipelineDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
