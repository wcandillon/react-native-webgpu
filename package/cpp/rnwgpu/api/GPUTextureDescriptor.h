#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUTextureDescriptor {
public:
  wgpu::TextureDescriptor *getInstance() { return &_instance; }

  wgpu::TextureDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUTextureDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "size")) {
        auto size = value.getProperty(runtime, "size");

        else if (size.isUndefined()) {
          throw std::runtime_error(
              "Property GPUTextureDescriptor::size is required");
        }
      }
      if (value.hasProperty(runtime, "mipLevelCount")) {
        auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");

        if (value.hasProperty(runtime, "mipLevelCount")) {
          result->_instance.mipLevelCount = mipLevelCount.getNumber();
        }
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto sampleCount = value.getProperty(runtime, "sampleCount");

        if (value.hasProperty(runtime, "sampleCount")) {
          result->_instance.sampleCount = sampleCount.getNumber();
        }
      }
      if (value.hasProperty(runtime, "dimension")) {
        auto dimension = value.getProperty(runtime, "dimension");
      }
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        else if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUTextureDescriptor::format is required");
        }
      }
      if (value.hasProperty(runtime, "usage")) {
        auto usage = value.getProperty(runtime, "usage");

        else if (usage.isUndefined()) {
          throw std::runtime_error(
              "Property GPUTextureDescriptor::usage is required");
        }
      }
      if (value.hasProperty(runtime, "viewFormats")) {
        auto viewFormats = value.getProperty(runtime, "viewFormats");
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
