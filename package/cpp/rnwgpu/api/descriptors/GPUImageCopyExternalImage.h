#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUOrigin2DDictStrict.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "Logger.h"
#include "OffscreenCanvas.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"
#include "VideoFrame.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUImageCopyExternalImage {
  std::variant<std::shared_ptr<ImageBitmap>, std::shared_ptr<ImageData>,
               std::shared_ptr<HTMLImageElement>,
               std::shared_ptr<HTMLVideoElement>, std::shared_ptr<VideoFrame>,
               std::shared_ptr<HTMLCanvasElement>,
               std::shared_ptr<OffscreenCanvas>>
      source; // GPUImageCopyExternalImageSource
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUOrigin2DDictStrict>>>
      origin;                // GPUOrigin2DStrict
  std::optional<bool> flipY; // boolean
};

bool conv(wgpu::ImageCopyExternalImage &out, GPUImageCopyExternalImage &in) {

  return conv(out.source, in.source) && conv(out.origin, in.origin) &&
         conv(out.flipY, in.flipY);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyExternalImage>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // source std::variant<std::shared_ptr<ImageBitmap>,
      // std::shared_ptr<ImageData>, std::shared_ptr<HTMLImageElement>,
      // std::shared_ptr<HTMLVideoElement>, std::shared_ptr<VideoFrame>,
      // std::shared_ptr<HTMLCanvasElement>, std::shared_ptr<OffscreenCanvas>>
      // origin std::optional<std::variant<std::vector<double>,
      // std::shared_ptr<GPUOrigin2DDictStrict>>> flipY std::optional<bool>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyExternalImage> arg) {
    throw std::runtime_error("Invalid GPUImageCopyExternalImage::toJSI()");
  }
};

} // namespace margelo