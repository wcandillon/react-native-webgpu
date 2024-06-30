#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUVertexBufferLayout {
public:
  wgpu::VertexBufferLayout *getInstance() { return &_instance; }

  wgpu::VertexBufferLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexBufferLayout>> {
  static std::shared_ptr<rnwgpu::GPUVertexBufferLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUVertexBufferLayout>();
    if (value.hasProperty(runtime, "arrayStride")) {
      auto arrayStride = value.getProperty(runtime, "arrayStride");

      else if (arrayStride.isUndefined()) {
        throw std::runtime_error(
            "Property GPUVertexBufferLayout::arrayStride is required");
      }
    }
    if (value.hasProperty(runtime, "stepMode")) {
      auto stepMode = value.getProperty(runtime, "stepMode");
    }
    if (value.hasProperty(runtime, "attributes")) {
      auto attributes = value.getProperty(runtime, "attributes");

      else if (attributes.isUndefined()) {
        throw std::runtime_error(
            "Property GPUVertexBufferLayout::attributes is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexBufferLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
