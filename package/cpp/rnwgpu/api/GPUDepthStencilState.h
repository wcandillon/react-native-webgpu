#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUDepthStencilState {
public:
  wgpu::DepthStencilState *getInstance() { return &_instance; }

private:
  wgpu::DepthStencilState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDepthStencilState>> {
  static std::shared_ptr<rnwgpu::GPUDepthStencilState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUDepthStencilState>();
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      }
    }
    if (value.hasProperty(runtime, "depthWriteEnabled")) {
      auto depthWriteEnabled = value.getProperty(runtime, "depthWriteEnabled");
      if (depthWriteEnabled.isNumber()) {
        result->_instance.depthWriteEnabled = depthWriteEnabled.getNumber();
      } else if (depthWriteEnabled.isNull() ||
                 depthWriteEnabled.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::depthWriteEnabled is required");
      }
    }
    if (value.hasProperty(runtime, "depthCompare")) {
      auto depthCompare = value.getProperty(runtime, "depthCompare");
      if (depthCompare.isNumber()) {
        result->_instance.depthCompare = depthCompare.getNumber();
      } else if (depthCompare.isNull() || depthCompare.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::depthCompare is required");
      }
    }
    if (value.hasProperty(runtime, "stencilFront")) {
      auto stencilFront = value.getProperty(runtime, "stencilFront");
      if (stencilFront.isNumber()) {
        result->_instance.stencilFront = stencilFront.getNumber();
      } else if (stencilFront.isNull() || stencilFront.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::stencilFront is required");
      }
    }
    if (value.hasProperty(runtime, "stencilBack")) {
      auto stencilBack = value.getProperty(runtime, "stencilBack");
      if (stencilBack.isNumber()) {
        result->_instance.stencilBack = stencilBack.getNumber();
      } else if (stencilBack.isNull() || stencilBack.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::stencilBack is required");
      }
    }
    if (value.hasProperty(runtime, "stencilReadMask")) {
      auto stencilReadMask = value.getProperty(runtime, "stencilReadMask");
      if (stencilReadMask.isNumber()) {
        result->_instance.stencilReadMask = stencilReadMask.getNumber();
      } else if (stencilReadMask.isNull() || stencilReadMask.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::stencilReadMask is required");
      }
    }
    if (value.hasProperty(runtime, "stencilWriteMask")) {
      auto stencilWriteMask = value.getProperty(runtime, "stencilWriteMask");
      if (stencilWriteMask.isNumber()) {
        result->_instance.stencilWriteMask = stencilWriteMask.getNumber();
      } else if (stencilWriteMask.isNull() || stencilWriteMask.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::stencilWriteMask is required");
      }
    }
    if (value.hasProperty(runtime, "depthBias")) {
      auto depthBias = value.getProperty(runtime, "depthBias");
      if (depthBias.isNumber()) {
        result->_instance.depthBias = depthBias.getNumber();
      } else if (depthBias.isNull() || depthBias.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::depthBias is required");
      }
    }
    if (value.hasProperty(runtime, "depthBiasSlopeScale")) {
      auto depthBiasSlopeScale =
          value.getProperty(runtime, "depthBiasSlopeScale");
      if (depthBiasSlopeScale.isNumber()) {
        result->_instance.depthBiasSlopeScale = depthBiasSlopeScale.getNumber();
      } else if (depthBiasSlopeScale.isNull() ||
                 depthBiasSlopeScale.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::depthBiasSlopeScale is required");
      }
    }
    if (value.hasProperty(runtime, "depthBiasClamp")) {
      auto depthBiasClamp = value.getProperty(runtime, "depthBiasClamp");
      if (depthBiasClamp.isNumber()) {
        result->_instance.depthBiasClamp = depthBiasClamp.getNumber();
      } else if (depthBiasClamp.isNull() || depthBiasClamp.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::depthBiasClamp is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDepthStencilState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
