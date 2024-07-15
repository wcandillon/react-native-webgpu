#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUBufferBindingLayout.h"
#include "GPUExternalTextureBindingLayout.h"
#include "GPUSamplerBindingLayout.h"
#include "GPUStorageTextureBindingLayout.h"
#include "GPUTextureBindingLayout.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUBindGroupLayoutEntry {
public:
  wgpu::BindGroupLayoutEntry *getInstance() { return &_instance; }

  wgpu::BindGroupLayoutEntry _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupLayoutEntry>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "binding")) {
        auto binding = value.getProperty(runtime, "binding");

        if (binding.isNumber()) {
          result->_instance.binding =
              static_cast<wgpu::Index32>(binding.getNumber());
        }

        if (binding.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupLayoutEntry::binding is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::binding is not defined");
      }
      if (value.hasProperty(runtime, "visibility")) {
        auto visibility = value.getProperty(runtime, "visibility");

        if (visibility.isNumber()) {
          result->_instance.visibility =
              static_cast<wgpu::ShaderStageFlags>(visibility.getNumber());
        }

        if (visibility.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupLayoutEntry::visibility is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupLayoutEntry::visibility is not defined");
      }
      if (value.hasProperty(runtime, "buffer")) {
        auto buffer = value.getProperty(runtime, "buffer");

        if (buffer.isObject()) {
          auto val = m::JSIConverter<
              std::shared_ptr<rnwgpu::GPUBufferBindingLayout>>::fromJSI(runtime,
                                                                        buffer,
                                                                        false);
          result->_instance.buffer = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "sampler")) {
        auto sampler = value.getProperty(runtime, "sampler");

        if (sampler.isObject()) {
          auto val = m::JSIConverter<std::shared_ptr<
              rnwgpu::GPUSamplerBindingLayout>>::fromJSI(runtime, sampler,
                                                         false);
          result->_instance.sampler = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "texture")) {
        auto texture = value.getProperty(runtime, "texture");

        if (texture.isObject()) {
          auto val = m::JSIConverter<std::shared_ptr<
              rnwgpu::GPUTextureBindingLayout>>::fromJSI(runtime, texture,
                                                         false);
          result->_instance.texture = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "storageTexture")) {
        auto storageTexture = value.getProperty(runtime, "storageTexture");

        if (storageTexture.isObject()) {
          auto val = m::JSIConverter<std::shared_ptr<
              rnwgpu::GPUStorageTextureBindingLayout>>::fromJSI(runtime,
                                                                storageTexture,
                                                                false);
          result->_instance.storageTexture = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "externalTexture")) {
        auto externalTexture = value.getProperty(runtime, "externalTexture");

        if (externalTexture.isObject()) {
          auto val = m::JSIConverter<
              std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout>>::
              fromJSI(runtime, externalTexture, false);
          result->_instance.externalTexture = val->_instance;
        }
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
