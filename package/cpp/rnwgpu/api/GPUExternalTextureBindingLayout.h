#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExternalTextureBindingLayout {
public:
  wgpu::ExternalTextureBindingLayout *getInstance() { return &_instance; }

  wgpu::ExternalTextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUExternalTextureBindingLayout>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
