#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExternalTextureDescriptor {
public:
  wgpu::ExternalTextureDescriptor *getInstance() { return &_instance; }

private:
  wgpu::ExternalTextureDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUExternalTextureDescriptor>();
    if (value.hasProperty(runtime, "source")) {
      auto source = value.getProperty(runtime, "source");
      if (source.isNumber()) {
        result->_instance.source = source.getNumber();
      }
    }
    if (value.hasProperty(runtime, "colorSpace")) {
      auto colorSpace = value.getProperty(runtime, "colorSpace");
      if (colorSpace.isNumber()) {
        result->_instance.colorSpace = colorSpace.getNumber();
      } else if (colorSpace.isNull() || colorSpace.isUndefined()) {
        throw std::runtime_error(
            "Property GPUExternalTextureDescriptor::colorSpace is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
