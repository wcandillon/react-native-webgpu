#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUOrigin3DDict.h"
#include "GPUTexture.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUImageCopyTextureTagged {
  std::optional<wgpu::definedColorSpace> colorSpace; // PredefinedColorSpace
  std::optional<bool> premultipliedAlpha;            // boolean
  std::shared_ptr<GPUTexture> texture;               // GPUTexture
  std::optional<double> mipLevel;                    // GPUIntegerCoordinate
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUOrigin3DDict>>>
      origin;                                // GPUOrigin3D
  std::optional<wgpu::TextureAspect> aspect; // GPUTextureAspect
};

bool conv(wgpu::ImageCopyTextureTagged &out,
          const GPUImageCopyTextureTagged &in) {

  return conv(out.colorSpace, in.colorSpace) &&
         conv(out.premultipliedAlpha, in.premultipliedAlpha) &&
         conv(out.texture, in.texture) && conv(out.mipLevel, in.mipLevel) &&
         conv(out.origin, in.origin) && conv(out.aspect, in.aspect);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTextureTagged>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // colorSpace std::optional<wgpu::definedColorSpace>
      // premultipliedAlpha std::optional<bool>
      // texture std::shared_ptr<GPUTexture>
      // mipLevel std::optional<double>
      // origin std::optional<std::variant<std::vector<double>,
      // std::shared_ptr<GPUOrigin3DDict>>> aspect
      // std::optional<wgpu::TextureAspect>
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