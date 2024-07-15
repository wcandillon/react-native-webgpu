#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUStorageTextureBindingLayout {
public:
  wgpu::StorageTextureBindingLayout *getInstance() { return &_instance; }

  wgpu::StorageTextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUStorageTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "access")) {
        auto access = value.getProperty(runtime, "access");

        if (access.isString()) {
          auto str = access.asString(runtime).utf8(runtime);
          wgpu::StorageTextureAccess enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.access = enumValue;
        }
      }
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isString()) {
          auto str = format.asString(runtime).utf8(runtime);
          wgpu::TextureFormat enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.format = enumValue;
        }

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUStorageTextureBindingLayout::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUStorageTextureBindingLayout::format is not defined");
      }
      if (value.hasProperty(runtime, "viewDimension")) {
        auto viewDimension = value.getProperty(runtime, "viewDimension");

        if (viewDimension.isString()) {
          auto str = viewDimension.asString(runtime).utf8(runtime);
          wgpu::TextureViewDimension enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.viewDimension = enumValue;
        }
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
