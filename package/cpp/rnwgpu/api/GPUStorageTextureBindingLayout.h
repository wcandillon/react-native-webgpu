#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUStorageTextureBindingLayout {
public:
  wgpu::StorageTextureBindingLayout *getInstance() { return &_instance; }

  wgpu::StorageTextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUStorageTextureBindingLayout>();
    if (value.hasProperty(runtime, "access")) {
      auto access = value.getProperty(runtime, "access");
    }
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");

      else if (format.isUndefined()) {
        throw std::runtime_error(
            "Property GPUStorageTextureBindingLayout::format is required");
      }
    }
    if (value.hasProperty(runtime, "viewDimension")) {
      auto viewDimension = value.getProperty(runtime, "viewDimension");
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
