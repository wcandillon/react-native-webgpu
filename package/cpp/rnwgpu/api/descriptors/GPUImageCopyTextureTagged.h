#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "External.h"
#include "GPUOrigin3D.h"
#include "GPUTexture.h"
#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUImageCopyTextureTagged {
  std::optional<PredefinedColorSpace> colorSpace;     // PredefinedColorSpace
  std::optional<bool> premultipliedAlpha;             // boolean
  std::shared_ptr<GPUTexture> texture;                // GPUTexture
  std::optional<double> mipLevel;                     // GPUIntegerCoordinate
  std::optional<std::shared_ptr<GPUOrigin3D>> origin; // GPUOrigin3D
  std::optional<wgpu::TextureAspect> aspect;          // GPUTextureAspect
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTextureTagged>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "colorSpace")) {
        auto prop = value.getProperty(runtime, "colorSpace");
        result->colorSpace =
            JSIConverter<std::optional<PredefinedColorSpace>>::fromJSI(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "premultipliedAlpha")) {
        auto prop = value.getProperty(runtime, "premultipliedAlpha");
        result->premultipliedAlpha =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "texture")) {
        auto prop = value.getProperty(runtime, "texture");
        result->texture = JSIConverter<std::shared_ptr<GPUTexture>>::fromJSI(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "mipLevel")) {
        auto prop = value.getProperty(runtime, "mipLevel");
        result->mipLevel =
            JSIConverter<std::optional<double>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "origin")) {
        auto prop = value.getProperty(runtime, "origin");
        result->origin =
            JSIConverter<std::optional<std::shared_ptr<GPUOrigin3D>>>::fromJSI(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "aspect")) {
        auto prop = value.getProperty(runtime, "aspect");
        result->aspect =
            JSIConverter<std::optional<wgpu::TextureAspect>>::fromJSI(
                runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged> arg) {
    throw std::runtime_error("Invalid GPUImageCopyTextureTagged::toJSI()");
  }
};

} // namespace margelo