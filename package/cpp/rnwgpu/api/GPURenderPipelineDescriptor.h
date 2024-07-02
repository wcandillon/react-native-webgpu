#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPipelineDescriptor {
public:
  wgpu::RenderPipelineDescriptor *getInstance() { return &_instance; }

  wgpu::RenderPipelineDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPURenderPipelineDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "vertex")) {
        auto vertex = value.getProperty(runtime, "vertex");

        else if (vertex.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPipelineDescriptor::vertex is required");
        }
      }
      if (value.hasProperty(runtime, "primitive")) {
        auto primitive = value.getProperty(runtime, "primitive");
      }
      if (value.hasProperty(runtime, "depthStencil")) {
        auto depthStencil = value.getProperty(runtime, "depthStencil");
      }
      if (value.hasProperty(runtime, "multisample")) {
        auto multisample = value.getProperty(runtime, "multisample");
      }
      if (value.hasProperty(runtime, "fragment")) {
        auto fragment = value.getProperty(runtime, "fragment");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
