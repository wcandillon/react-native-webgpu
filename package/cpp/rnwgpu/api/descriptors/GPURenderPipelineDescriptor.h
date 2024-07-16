#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUDepthStencilState.h"
#include "GPUFragmentState.h"
#include "GPUMultisampleState.h"
#include "GPUPrimitiveState.h"
#include "GPUVertexState.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPURenderPipelineDescriptor {
public:
  wgpu::RenderPipelineDescriptor *getInstance() { return &_instance; }

  wgpu::RenderPipelineDescriptor _instance;

  std::string label;
  std::shared_ptr<rnwgpu::GPUFragmentState> fragmentState;
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

        if (vertex.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUVertexState>>::fromJSI(
                  runtime, vertex, false);
          result->_instance.vertex = val->_instance;
        }

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

        if (primitive.isObject()) {
          auto val = m::JSIConverter<
              std::shared_ptr<rnwgpu::GPUPrimitiveState>>::fromJSI(runtime,
                                                                   primitive,
                                                                   false);
          result->_instance.primitive = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "depthStencil")) {
        auto depthStencil = value.getProperty(runtime, "depthStencil");

        if (depthStencil.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUDepthStencilState>>::
                  fromJSI(runtime, depthStencil, false);
          result->_instance.depthStencil = val->getInstance();
        }
      }
      if (value.hasProperty(runtime, "multisample")) {
        auto multisample = value.getProperty(runtime, "multisample");

        if (multisample.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUMultisampleState>>::
                  fromJSI(runtime, multisample, false);
          result->_instance.multisample = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "fragment")) {
        auto fragment = value.getProperty(runtime, "fragment");

        if (fragment.isObject()) {
          auto val = m::JSIConverter<
              std::shared_ptr<rnwgpu::GPUFragmentState>>::fromJSI(runtime,
                                                                  fragment,
                                                                  false);
          result->fragmentState = val;
          result->_instance.fragment = val->getInstance();
        }
      }
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        if (layout.isString()) {
          auto str = layout.asString(runtime).utf8(runtime);
          if (str == "auto") {
            result->_instance.layout = nullptr;
          } else {
            throw std::runtime_error(
                "Property GPURenderPipelineDescriptor::layout is invalid");
          }
        }
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
