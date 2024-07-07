#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

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
          result->_instance.binding = binding.getNumber();
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
          result->_instance.visibility = visibility.getNumber();
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
      }
      if (value.hasProperty(runtime, "sampler")) {
        auto sampler = value.getProperty(runtime, "sampler");
      }
      if (value.hasProperty(runtime, "texture")) {
        auto texture = value.getProperty(runtime, "texture");
      }
      if (value.hasProperty(runtime, "storageTexture")) {
        auto storageTexture = value.getProperty(runtime, "storageTexture");
      }
      if (value.hasProperty(runtime, "externalTexture")) {
        auto externalTexture = value.getProperty(runtime, "externalTexture");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for
    // GPUBindGroupLayoutEntry");
    //}
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
