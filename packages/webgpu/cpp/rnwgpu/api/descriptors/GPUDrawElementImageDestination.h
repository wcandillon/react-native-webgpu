#pragma once

#include <memory>
#include <optional>

#include "JSIConverter.h"

#include "GPUExtent3D.h"
#include "GPUImageCopyTextureTagged.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// The `destination` argument of GPUQueue.drawElementImageToTexture:
//   dictionary GPUDrawElementImageDestination : GPUCopyExternalImageDestInfo {
//     GPUExtent3D size;
//   };
// `size` is the pixel size of the region the view is drawn into. When omitted
// it is the source rectangle's natural pixel size (points times the device
// pixel ratio).
struct GPUDrawElementImageDestination {
  std::shared_ptr<GPUImageCopyTextureTagged> info;
  std::optional<std::shared_ptr<GPUExtent3D>> size;
};

} // namespace rnwgpu

namespace rnwgpu {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUDrawElementImageDestination>> {
  static std::shared_ptr<rnwgpu::GPUDrawElementImageDestination>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDrawElementImageDestination>();
    if (outOfBounds || !arg.isObject()) {
      throw std::runtime_error(
          "drawElementImageToTexture: expected a destination dictionary");
    }
    result->info =
        JSIConverter<std::shared_ptr<GPUImageCopyTextureTagged>>::fromJSI(
            runtime, arg, false);
    auto obj = arg.getObject(runtime);
    if (obj.hasProperty(runtime, "size")) {
      result->size =
          JSIConverter<std::optional<std::shared_ptr<GPUExtent3D>>>::fromJSI(
              runtime, obj.getProperty(runtime, "size"), false);
    }
    return result;
  }

  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUDrawElementImageDestination> arg) {
    throw std::runtime_error(
        "Invalid GPUDrawElementImageDestination::toJSI()");
  }
};

} // namespace rnwgpu
