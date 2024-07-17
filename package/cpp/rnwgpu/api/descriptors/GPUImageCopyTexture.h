#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUOrigin3DDict.h"
#include "GPUTexture.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUImageCopyTexture {
  std::shared_ptr<GPUTexture> texture; // GPUTexture
  std::optional<double> mipLevel;      // GPUIntegerCoordinate
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUOrigin3DDict>>>
      origin;                                // GPUOrigin3D
  std::optional<wgpu::TextureAspect> aspect; // GPUTextureAspect
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
        auto prop = value.getProperty(runtime, "texture");
        result->texture = JSIConverter::fromJSI<std::shared_ptr<GPUTexture>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "mipLevel")) {
        auto prop = value.getProperty(runtime, "mipLevel");
        result->mipLevel =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "origin")) {
        auto prop = value.getProperty(runtime, "origin");
        result->origin = JSIConverter::fromJSI<std::optional<std::variant<
            std::vector<double>, std::shared_ptr<GPUOrigin3DDict>>>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "aspect")) {
        auto prop = value.getProperty(runtime, "aspect");
        result->aspect =
            JSIConverter::fromJSI<std::optional<wgpu::TextureAspect>>(
                runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyTexture> arg) {
    throw std::runtime_error("Invalid GPUImageCopyTexture::toJSI()");
  }
};
} // namespace margelo