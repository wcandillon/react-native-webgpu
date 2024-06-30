#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUStencilFaceState {
public:
  wgpu::StencilFaceState *getInstance() { return &_instance; }

private:
  wgpu::StencilFaceState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUStencilFaceState>> {
  static std::shared_ptr<rnwgpu::GPUStencilFaceState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUStencilFaceState>();
    if (value.hasProperty(runtime, "compare")) {
      auto compare = value.getProperty(runtime, "compare");
      if (compare.isNumber()) {
        result->_instance.compare = compare.getNumber();
      } else if (compare.isNull() || compare.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStencilFaceState::compare is required");
      }
    }
    if (value.hasProperty(runtime, "failOp")) {
      auto failOp = value.getProperty(runtime, "failOp");
      if (failOp.isNumber()) {
        result->_instance.failOp = failOp.getNumber();
      } else if (failOp.isNull() || failOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStencilFaceState::failOp is required");
      }
    }
    if (value.hasProperty(runtime, "depthFailOp")) {
      auto depthFailOp = value.getProperty(runtime, "depthFailOp");
      if (depthFailOp.isNumber()) {
        result->_instance.depthFailOp = depthFailOp.getNumber();
      } else if (depthFailOp.isNull() || depthFailOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStencilFaceState::depthFailOp is required");
      }
    }
    if (value.hasProperty(runtime, "passOp")) {
      auto passOp = value.getProperty(runtime, "passOp");
      if (passOp.isNumber()) {
        result->_instance.passOp = passOp.getNumber();
      } else if (passOp.isNull() || passOp.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStencilFaceState::passOp is required");
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
