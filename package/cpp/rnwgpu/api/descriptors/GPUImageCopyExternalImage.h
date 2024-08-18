#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

#include "GPUOrigin2D.h"
#include "ImageData.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUImageCopyExternalImage {
  //std::shared_ptr<ImageData> source; // GPUImageCopyExternalImageSource
  std::optional<std::shared_ptr<GPUOrigin2D>> origin; // GPUOrigin2DStrict
  std::optional<bool> flipY;                          // boolean
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyExternalImage>();
    if (!outOfBounds && arg.isObject()) {
      auto obj = arg.getObject(runtime);
      if (obj.hasProperty(runtime, "source")) {
//        auto prop = obj.getProperty(runtime, "source");
//        result->source = JSIConverter<std::shared_ptr<ImageData>>::fromJSI(
//            runtime, prop, false);
      }
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
