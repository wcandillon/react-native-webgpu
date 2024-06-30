#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUStorageTextureBindingLayout {
public:
  wgpu::StorageTextureBindingLayout *getInstance() { return &_instance; }

private:
  wgpu::StorageTextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUStorageTextureBindingLayout>();
    if (value.hasProperty(runtime, "access")) {
      auto access = value.getProperty(runtime, "access");
      if (access.isNumber()) {
        result->_instance.access = access.getNumber();
      } else if (access.isNull() || access.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStorageTextureBindingLayout::access is required");
      }
    }
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      }
    }
    if (value.hasProperty(runtime, "viewDimension")) {
      auto viewDimension = value.getProperty(runtime, "viewDimension");
      if (viewDimension.isNumber()) {
        result->_instance.viewDimension = viewDimension.getNumber();
      } else if (viewDimension.isNull() || viewDimension.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStorageTextureBindingLayout::viewDimension is "
            "required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
