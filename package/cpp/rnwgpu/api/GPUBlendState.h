#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBlendState {
public:
  wgpu::BlendState *getInstance() { return &_instance; }

private:
  wgpu::BlendState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendState>> {
  static std::shared_ptr<rnwgpu::GPUBlendState> fromJSI(jsi::Runtime &runtime,
                                                        const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBlendState>();
    if (value.hasProperty(runtime, "color")) {
      auto color = value.getProperty(runtime, "color");
      if (color.isNumber()) {
        result->_instance.color = color.getNumber();
      }
    }
    if (value.hasProperty(runtime, "alpha")) {
      auto alpha = value.getProperty(runtime, "alpha");
      if (alpha.isNumber()) {
        result->_instance.alpha = alpha.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
