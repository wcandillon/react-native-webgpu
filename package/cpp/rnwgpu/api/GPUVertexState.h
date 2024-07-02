#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUVertexState {
public:
  wgpu::VertexState *getInstance() { return &_instance; }

  wgpu::VertexState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexState>> {
  static std::shared_ptr<rnwgpu::GPUVertexState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUVertexState>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "buffers")) {
        auto buffers = value.getProperty(runtime, "buffers");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
