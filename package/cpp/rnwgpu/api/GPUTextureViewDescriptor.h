#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureViewDescriptor {
public:
  wgpu::TextureViewDescriptor *getInstance() { return &_instance; }

private:
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
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      } else if (format.isNull() || format.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::format is required");
      }
    }
    if (value.hasProperty(runtime, "dimension")) {
      auto dimension = value.getProperty(runtime, "dimension");
      if (dimension.isNumber()) {
        result->_instance.dimension = dimension.getNumber();
      } else if (dimension.isNull() || dimension.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::dimension is required");
      }
    }
    if (value.hasProperty(runtime, "aspect")) {
      auto aspect = value.getProperty(runtime, "aspect");
      if (aspect.isNumber()) {
        result->_instance.aspect = aspect.getNumber();
      } else if (aspect.isNull() || aspect.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::aspect is required");
      }
    }
    if (value.hasProperty(runtime, "baseMipLevel")) {
      auto baseMipLevel = value.getProperty(runtime, "baseMipLevel");
      if (baseMipLevel.isNumber()) {
        result->_instance.baseMipLevel = baseMipLevel.getNumber();
      } else if (baseMipLevel.isNull() || baseMipLevel.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::baseMipLevel is required");
      }
    }
    if (value.hasProperty(runtime, "mipLevelCount")) {
      auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");
      if (mipLevelCount.isNumber()) {
        result->_instance.mipLevelCount = mipLevelCount.getNumber();
      } else if (mipLevelCount.isNull() || mipLevelCount.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::mipLevelCount is required");
      }
    }
    if (value.hasProperty(runtime, "baseArrayLayer")) {
      auto baseArrayLayer = value.getProperty(runtime, "baseArrayLayer");
      if (baseArrayLayer.isNumber()) {
        result->_instance.baseArrayLayer = baseArrayLayer.getNumber();
      } else if (baseArrayLayer.isNull() || baseArrayLayer.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::baseArrayLayer is required");
      }
    }
    if (value.hasProperty(runtime, "arrayLayerCount")) {
      auto arrayLayerCount = value.getProperty(runtime, "arrayLayerCount");
      if (arrayLayerCount.isNumber()) {
        result->_instance.arrayLayerCount = arrayLayerCount.getNumber();
      } else if (arrayLayerCount.isNull() || arrayLayerCount.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureViewDescriptor::arrayLayerCount is required");
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
