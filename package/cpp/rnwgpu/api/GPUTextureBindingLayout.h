#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureBindingLayout {
public:
  wgpu::TextureBindingLayout *getInstance() { return &_instance; }

private:
  wgpu::TextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUTextureBindingLayout>();
    if (value.hasProperty(runtime, "sampleType")) {
      auto sampleType = value.getProperty(runtime, "sampleType");
      if (sampleType.isNumber()) {
        result->_instance.sampleType = sampleType.getNumber();
      } else if (sampleType.isNull() || sampleType.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureBindingLayout::sampleType is required");
      }
    }
    if (value.hasProperty(runtime, "viewDimension")) {
      auto viewDimension = value.getProperty(runtime, "viewDimension");
      if (viewDimension.isNumber()) {
        result->_instance.viewDimension = viewDimension.getNumber();
      } else if (viewDimension.isNull() || viewDimension.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureBindingLayout::viewDimension is required");
      }
    }
    if (value.hasProperty(runtime, "multisampled")) {
      auto multisampled = value.getProperty(runtime, "multisampled");
      if (multisampled.isNumber()) {
        result->_instance.multisampled = multisampled.getNumber();
      } else if (multisampled.isNull() || multisampled.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureBindingLayout::multisampled is required");
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
