#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUStencilFaceState {
public:
  wgpu::StencilFaceState *getInstance() { return &_instance; }

  wgpu::StencilFaceState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUStencilFaceState>> {
  static std::shared_ptr<rnwgpu::GPUStencilFaceState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUStencilFaceState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "compare")) {
        auto compare = value.getProperty(runtime, "compare");
      }
      if (value.hasProperty(runtime, "failOp")) {
        auto failOp = value.getProperty(runtime, "failOp");
      }
      if (value.hasProperty(runtime, "depthFailOp")) {
        auto depthFailOp = value.getProperty(runtime, "depthFailOp");
      }
      if (value.hasProperty(runtime, "passOp")) {
        auto passOp = value.getProperty(runtime, "passOp");
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUStencilFaceState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
