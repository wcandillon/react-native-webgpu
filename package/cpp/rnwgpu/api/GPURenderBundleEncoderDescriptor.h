#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderBundleEncoderDescriptor {
public:
  wgpu::RenderBundleEncoderDescriptor *getInstance() { return &_instance; }

  wgpu::RenderBundleEncoderDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPURenderBundleEncoderDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depthReadOnly")) {
        auto depthReadOnly = value.getProperty(runtime, "depthReadOnly");
        if (value.hasProperty(runtime, "depthReadOnly")) {
          result->_instance.depthReadOnly = depthReadOnly.getBool();
        }
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
        if (value.hasProperty(runtime, "stencilReadOnly")) {
          result->_instance.stencilReadOnly = stencilReadOnly.getBool();
        }
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
