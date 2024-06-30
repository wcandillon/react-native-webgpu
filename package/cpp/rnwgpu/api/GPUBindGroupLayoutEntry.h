#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBindGroupLayoutEntry {
public:
  wgpu::BindGroupLayoutEntry *getInstance() { return &_instance; }

private:
  wgpu::BindGroupLayoutEntry _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBindGroupLayoutEntry>();
    if (value.hasProperty(runtime, "binding")) {
      auto binding = value.getProperty(runtime, "binding");
      if (binding.isNumber()) {
        result->_instance.binding = binding.getNumber();
      }
    }
    if (value.hasProperty(runtime, "visibility")) {
      auto visibility = value.getProperty(runtime, "visibility");
      if (visibility.isNumber()) {
        result->_instance.visibility = visibility.getNumber();
      }
    }
    if (value.hasProperty(runtime, "buffer")) {
      auto buffer = value.getProperty(runtime, "buffer");
      if (buffer.isNumber()) {
        result->_instance.buffer = buffer.getNumber();
      } else if (buffer.isNull() || buffer.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::buffer is required");
      }
    }
    if (value.hasProperty(runtime, "sampler")) {
      auto sampler = value.getProperty(runtime, "sampler");
      if (sampler.isNumber()) {
        result->_instance.sampler = sampler.getNumber();
      } else if (sampler.isNull() || sampler.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::sampler is required");
      }
    }
    if (value.hasProperty(runtime, "texture")) {
      auto texture = value.getProperty(runtime, "texture");
      if (texture.isNumber()) {
        result->_instance.texture = texture.getNumber();
      } else if (texture.isNull() || texture.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::texture is required");
      }
    }
    if (value.hasProperty(runtime, "storageTexture")) {
      auto storageTexture = value.getProperty(runtime, "storageTexture");
      if (storageTexture.isNumber()) {
        result->_instance.storageTexture = storageTexture.getNumber();
      } else if (storageTexture.isNull() || storageTexture.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::storageTexture is required");
      }
    }
    if (value.hasProperty(runtime, "externalTexture")) {
      auto externalTexture = value.getProperty(runtime, "externalTexture");
      if (externalTexture.isNumber()) {
        result->_instance.externalTexture = externalTexture.getNumber();
      } else if (externalTexture.isNull() || externalTexture.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::externalTexture is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
