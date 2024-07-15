#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUTextureViewDescriptor {
public:
  wgpu::TextureViewDescriptor *getInstance() { return &_instance; }

  wgpu::TextureViewDescriptor _instance;

  std::string label;
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

        if (baseMipLevel.isNumber()) {
          result->_instance.baseMipLevel =
              static_cast<wgpu::IntegerCoordinate>(baseMipLevel.getNumber());
        }
      }
      if (value.hasProperty(runtime, "mipLevelCount")) {
        auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");

        if (mipLevelCount.isNumber()) {
          result->_instance.mipLevelCount =
              static_cast<wgpu::IntegerCoordinate>(mipLevelCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "baseArrayLayer")) {
        auto baseArrayLayer = value.getProperty(runtime, "baseArrayLayer");

        if (baseArrayLayer.isNumber()) {
          result->_instance.baseArrayLayer =
              static_cast<wgpu::IntegerCoordinate>(baseArrayLayer.getNumber());
        }
      }
      if (value.hasProperty(runtime, "arrayLayerCount")) {
        auto arrayLayerCount = value.getProperty(runtime, "arrayLayerCount");

        if (arrayLayerCount.isNumber()) {
          result->_instance.arrayLayerCount =
              static_cast<wgpu::IntegerCoordinate>(arrayLayerCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str;
          result->_instance.label = result->label.c_str();
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
