#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBindGroupDescriptor {
public:
  wgpu::BindGroupDescriptor *getInstance() { return &_instance; }

  wgpu::BindGroupDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        else if (layout.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupDescriptor::layout is required");
        }
      }
      if (value.hasProperty(runtime, "entries")) {
        auto entries = value.getProperty(runtime, "entries");

        else if (entries.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupDescriptor::entries is required");
        }
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
