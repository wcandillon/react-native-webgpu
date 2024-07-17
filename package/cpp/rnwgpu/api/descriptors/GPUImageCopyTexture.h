#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUOrigin3D.h"
#include "GPUTexture.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUImageCopyTexture {
  std::shared_ptr<GPUTexture> texture;       // GPUTexture
  std::optional<double> mipLevel;            // GPUIntegerCoordinate
  std::optional<GPUOrigin3D> origin;         // GPUOrigin3D
  std::optional<wgpu::TextureAspect> aspect; // GPUTextureAspect
};

bool conv(wgpu::ImageCopyTexture &out, const GPUImageCopyTexture &in) {

  return conv(out.texture, in.texture) && conv(out.mipLevel, in.mipLevel) &&
         conv(out.origin, in.origin) && conv(out.aspect, in.aspect);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTexture>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTexture>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTexture>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // texture std::shared_ptr<GPUTexture>
      // mipLevel std::optional<double>
      // origin std::optional<GPUOrigin3D>
      // aspect std::optional<wgpu::TextureAspect>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyTexture> arg) {
    throw std::runtime_error("Invalid GPUImageCopyTexture::toJSI()");
  }
};

} // namespace margelo