#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUTextureViewDescriptor {
  std::optional<wgpu::TextureFormat> format; // GPUTextureFormat
  std::optional<wgpu::TextureViewDimension>
      dimension;                             // GPUTextureViewDimension
  std::optional<wgpu::TextureAspect> aspect; // GPUTextureAspect
  std::optional<double> baseMipLevel;        // GPUIntegerCoordinate
  std::optional<double> mipLevelCount;       // GPUIntegerCoordinate
  std::optional<double> baseArrayLayer;      // GPUIntegerCoordinate
  std::optional<double> arrayLayerCount;     // GPUIntegerCoordinate
  std::optional<std::string> label;          // string
};

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureViewDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureViewDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo