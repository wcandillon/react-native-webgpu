#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUDepthStencilState {
public:
  wgpu::DepthStencilState *getInstance() { return &_instance; }

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

      else if (format.isUndefined()) {
        throw std::runtime_error(
            "Property GPUDepthStencilState::format is required");
      }
    }
    if (value.hasProperty(runtime, "depthWriteEnabled")) {
      auto depthWriteEnabled = value.getProperty(runtime, "depthWriteEnabled");
      if (value.hasProperty(runtime, "depthWriteEnabled")) {
        result->_instance.depthWriteEnabled = depthWriteEnabled.getBool();
      }
    }
    if (value.hasProperty(runtime, "depthCompare")) {
      auto depthCompare = value.getProperty(runtime, "depthCompare");
    }
    if (value.hasProperty(runtime, "stencilFront")) {
      auto stencilFront = value.getProperty(runtime, "stencilFront");
    }
    if (value.hasProperty(runtime, "stencilBack")) {
      auto stencilBack = value.getProperty(runtime, "stencilBack");
    }
    if (value.hasProperty(runtime, "stencilReadMask")) {
      auto stencilReadMask = value.getProperty(runtime, "stencilReadMask");
    }
    if (value.hasProperty(runtime, "stencilWriteMask")) {
      auto stencilWriteMask = value.getProperty(runtime, "stencilWriteMask");
    }
    if (value.hasProperty(runtime, "depthBias")) {
      auto depthBias = value.getProperty(runtime, "depthBias");
    }
    if (value.hasProperty(runtime, "depthBiasSlopeScale")) {
      auto depthBiasSlopeScale =
          value.getProperty(runtime, "depthBiasSlopeScale");
    }
    if (value.hasProperty(runtime, "depthBiasClamp")) {
      auto depthBiasClamp = value.getProperty(runtime, "depthBiasClamp");
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
