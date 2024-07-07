#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureViewDescriptor {
public:
  wgpu::TextureViewDescriptor *getInstance() { return &_instance; }

  wgpu::TextureViewDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureViewDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
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

        if (value.hasProperty(runtime, "baseMipLevel")) {
          result->_instance.baseMipLevel = baseMipLevel.getNumber();
        }
      }
      if (value.hasProperty(runtime, "mipLevelCount")) {
        auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");

        if (value.hasProperty(runtime, "mipLevelCount")) {
          result->_instance.mipLevelCount = mipLevelCount.getNumber();
        }
      }
      if (value.hasProperty(runtime, "baseArrayLayer")) {
        auto baseArrayLayer = value.getProperty(runtime, "baseArrayLayer");

        if (value.hasProperty(runtime, "baseArrayLayer")) {
          result->_instance.baseArrayLayer = baseArrayLayer.getNumber();
        }
      }
      if (value.hasProperty(runtime, "arrayLayerCount")) {
        auto arrayLayerCount = value.getProperty(runtime, "arrayLayerCount");

        if (value.hasProperty(runtime, "arrayLayerCount")) {
          result->_instance.arrayLayerCount = arrayLayerCount.getNumber();
        }
      }
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
