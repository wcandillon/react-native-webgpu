#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureViewDescriptor {
public:
  wgpu::TextureViewDescriptor *getInstance() { return &_instance; }

  wgpu::TextureViewDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUTextureViewDescriptor>();
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
    }
    if (value.hasProperty(runtime, "dimension")) {
      auto dimension = value.getProperty(runtime, "dimension");
    }
    if (value.hasProperty(runtime, "aspect")) {
      auto aspect = value.getProperty(runtime, "aspect");
    }
    if (value.hasProperty(runtime, "baseMipLevel")) {
      auto baseMipLevel = value.getProperty(runtime, "baseMipLevel");
    }
    if (value.hasProperty(runtime, "mipLevelCount")) {
      auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");
    }
    if (value.hasProperty(runtime, "baseArrayLayer")) {
      auto baseArrayLayer = value.getProperty(runtime, "baseArrayLayer");
    }
    if (value.hasProperty(runtime, "arrayLayerCount")) {
      auto arrayLayerCount = value.getProperty(runtime, "arrayLayerCount");
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureViewDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
