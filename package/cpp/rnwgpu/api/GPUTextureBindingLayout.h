#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureBindingLayout {
public:
  wgpu::TextureBindingLayout *getInstance() { return &_instance; }

  wgpu::TextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "sampleType")) {
        auto sampleType = value.getProperty(runtime, "sampleType");
      }
      if (value.hasProperty(runtime, "viewDimension")) {
        auto viewDimension = value.getProperty(runtime, "viewDimension");
      }
      if (value.hasProperty(runtime, "multisampled")) {
        auto multisampled = value.getProperty(runtime, "multisampled");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for
    // GPUTextureBindingLayout");
    //}
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
