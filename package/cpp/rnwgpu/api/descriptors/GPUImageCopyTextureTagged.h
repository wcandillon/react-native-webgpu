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

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTextureTagged>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo