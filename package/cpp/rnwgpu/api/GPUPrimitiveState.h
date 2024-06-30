#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPrimitiveState {
public:
  wgpu::PrimitiveState *getInstance() { return &_instance; }

private:
  wgpu::PrimitiveState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPrimitiveState>> {
  static std::shared_ptr<rnwgpu::GPUPrimitiveState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUPrimitiveState>();
    if (value.hasProperty(runtime, "topology")) {
      auto topology = value.getProperty(runtime, "topology");
      if (topology.isNumber()) {
        result->_instance.topology = topology.getNumber();
      } else if (topology.isNull() || topology.isUndefined()) {
        throw std::runtime_error(
            "Property GPUPrimitiveState::topology is required");
      }
    }
    if (value.hasProperty(runtime, "stripIndexFormat")) {
      auto stripIndexFormat = value.getProperty(runtime, "stripIndexFormat");
      if (stripIndexFormat.isNumber()) {
        result->_instance.stripIndexFormat = stripIndexFormat.getNumber();
      } else if (stripIndexFormat.isNull() || stripIndexFormat.isUndefined()) {
        throw std::runtime_error(
            "Property GPUPrimitiveState::stripIndexFormat is required");
      }
    }
    if (value.hasProperty(runtime, "frontFace")) {
      auto frontFace = value.getProperty(runtime, "frontFace");
      if (frontFace.isNumber()) {
        result->_instance.frontFace = frontFace.getNumber();
      } else if (frontFace.isNull() || frontFace.isUndefined()) {
        throw std::runtime_error(
            "Property GPUPrimitiveState::frontFace is required");
      }
    }
    if (value.hasProperty(runtime, "cullMode")) {
      auto cullMode = value.getProperty(runtime, "cullMode");
      if (cullMode.isNumber()) {
        result->_instance.cullMode = cullMode.getNumber();
      } else if (cullMode.isNull() || cullMode.isUndefined()) {
        throw std::runtime_error(
            "Property GPUPrimitiveState::cullMode is required");
      }
    }
    if (value.hasProperty(runtime, "unclippedDepth")) {
      auto unclippedDepth = value.getProperty(runtime, "unclippedDepth");
      if (unclippedDepth.isNumber()) {
        result->_instance.unclippedDepth = unclippedDepth.getNumber();
      } else if (unclippedDepth.isNull() || unclippedDepth.isUndefined()) {
        throw std::runtime_error(
            "Property GPUPrimitiveState::unclippedDepth is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUPrimitiveState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
