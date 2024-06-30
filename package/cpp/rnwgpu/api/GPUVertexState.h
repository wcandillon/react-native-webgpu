#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUVertexState {
public:
  wgpu::VertexState *getInstance() { return &_instance; }

private:
  wgpu::VertexState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexState>> {
  static std::shared_ptr<rnwgpu::GPUVertexState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUVertexState>();
    if (value.hasProperty(runtime, "buffers")) {
      auto buffers = value.getProperty(runtime, "buffers");
      if (buffers.isNumber()) {
        result->_instance.buffers = buffers.getNumber();
      } else if (buffers.isNull() || buffers.isUndefined()) {
        throw std::runtime_error(
            "Property GPUVertexState::buffers is required");
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
