#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureBindingLayout {
public:
  wgpu::TextureBindingLayout *getInstance() { return &_instance; }

  wgpu::TextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUTextureBindingLayout>();
    if (value.hasProperty(runtime, "sampleType")) {
      auto sampleType = value.getProperty(runtime, "sampleType");
    }
    if (value.hasProperty(runtime, "viewDimension")) {
      auto viewDimension = value.getProperty(runtime, "viewDimension");
    }
    if (value.hasProperty(runtime, "multisampled")) {
      auto multisampled = value.getProperty(runtime, "multisampled");
      if (value.hasProperty(runtime, "multisampled")) {
        result->_instance.multisampled = multisampled.getBool();
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
