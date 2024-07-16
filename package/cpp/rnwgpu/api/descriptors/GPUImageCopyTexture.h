#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUOrigin3D.h"
#include "GPUTexture.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUImageCopyTexture {
public:
  wgpu::ImageCopyTexture *getInstance() { return &_instance; }

  wgpu::ImageCopyTexture _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTexture>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTexture>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTexture>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "texture")) {
        auto texture = value.getProperty(runtime, "texture");

        if (texture.isObject() &&
            texture.getObject(runtime).isHostObject(runtime)) {
          result->_instance.texture =
              texture.getObject(runtime)
                  .asHostObject<rnwgpu::GPUTexture>(runtime)
                  ->get();
        }

        if (texture.isUndefined()) {
          throw std::runtime_error(
              "Property GPUImageCopyTexture::texture is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUImageCopyTexture::texture is not defined");
      }
      if (value.hasProperty(runtime, "mipLevel")) {
        auto mipLevel = value.getProperty(runtime, "mipLevel");

        if (mipLevel.isNumber()) {
          result->_instance.mipLevel =
              static_cast<uint32_t>(mipLevel.getNumber());
        }
      }
      if (value.hasProperty(runtime, "origin")) {
        auto origin = value.getProperty(runtime, "origin");

        if (origin.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin3D>>::fromJSI(
                  runtime, origin, false);
          result->_instance.origin = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "aspect")) {
        auto aspect = value.getProperty(runtime, "aspect");

        if (aspect.isString()) {
          auto str = aspect.asString(runtime).utf8(runtime);
          wgpu::TextureAspect enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.aspect = enumValue;
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyTexture> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
