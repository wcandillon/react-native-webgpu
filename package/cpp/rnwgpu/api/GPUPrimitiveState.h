#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPrimitiveState {
public:
  wgpu::PrimitiveState *getInstance() { return &_instance; }

  wgpu::PrimitiveState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPrimitiveState>> {
  static std::shared_ptr<rnwgpu::GPUPrimitiveState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUPrimitiveState>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "topology")) {
        auto topology = value.getProperty(runtime, "topology");
      }
      if (value.hasProperty(runtime, "stripIndexFormat")) {
        auto stripIndexFormat = value.getProperty(runtime, "stripIndexFormat");
      }
      if (value.hasProperty(runtime, "frontFace")) {
        auto frontFace = value.getProperty(runtime, "frontFace");
      }
      if (value.hasProperty(runtime, "cullMode")) {
        auto cullMode = value.getProperty(runtime, "cullMode");
      }
      if (value.hasProperty(runtime, "unclippedDepth")) {
        auto unclippedDepth = value.getProperty(runtime, "unclippedDepth");
        if (value.hasProperty(runtime, "unclippedDepth")) {
          result->_instance.unclippedDepth = unclippedDepth.getBool();
        }
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
