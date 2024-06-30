#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureDescriptor {
public:
  wgpu::TextureDescriptor *getInstance() { return &_instance; }

private:
  wgpu::TextureDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUTextureDescriptor>();
    if (value.hasProperty(runtime, "size")) {
      auto size = value.getProperty(runtime, "size");
      if (size.isNumber()) {
        result->_instance.size = size.getNumber();
      }
    }
    if (value.hasProperty(runtime, "mipLevelCount")) {
      auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");
      if (mipLevelCount.isNumber()) {
        result->_instance.mipLevelCount = mipLevelCount.getNumber();
      } else if (mipLevelCount.isNull() || mipLevelCount.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::mipLevelCount is required");
      }
    }
    if (value.hasProperty(runtime, "sampleCount")) {
      auto sampleCount = value.getProperty(runtime, "sampleCount");
      if (sampleCount.isNumber()) {
        result->_instance.sampleCount = sampleCount.getNumber();
      } else if (sampleCount.isNull() || sampleCount.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::sampleCount is required");
      }
    }
    if (value.hasProperty(runtime, "dimension")) {
      auto dimension = value.getProperty(runtime, "dimension");
      if (dimension.isNumber()) {
        result->_instance.dimension = dimension.getNumber();
      } else if (dimension.isNull() || dimension.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::dimension is required");
      }
    }
    if (value.hasProperty(runtime, "format")) {
      auto format = value.getProperty(runtime, "format");
      if (format.isNumber()) {
        result->_instance.format = format.getNumber();
      }
    }
    if (value.hasProperty(runtime, "usage")) {
      auto usage = value.getProperty(runtime, "usage");
      if (usage.isNumber()) {
        result->_instance.usage = usage.getNumber();
      }
    }
    if (value.hasProperty(runtime, "viewFormats")) {
      auto viewFormats = value.getProperty(runtime, "viewFormats");
      if (viewFormats.isNumber()) {
        result->_instance.viewFormats = viewFormats.getNumber();
      } else if (viewFormats.isNull() || viewFormats.isUndefined()) {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::viewFormats is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUTextureDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
