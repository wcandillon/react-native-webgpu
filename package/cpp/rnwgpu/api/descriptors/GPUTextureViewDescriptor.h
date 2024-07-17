#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
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

static bool conv(wgpu::TextureViewDescriptor &out,
                 std::shared_ptr<GPUTextureViewDescriptor> &in) {

  return conv(out.format, in->format) && conv(out.dimension, in->dimension) &&
         conv(out.aspect, in->aspect) &&
         conv(out.baseMipLevel, in->baseMipLevel) &&
         conv(out.mipLevelCount, in->mipLevelCount) &&
         conv(out.baseArrayLayer, in->baseArrayLayer) &&
         conv(out.arrayLayerCount, in->arrayLayerCount) &&
         conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureViewDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureViewDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // format std::optional<wgpu::TextureFormat>
      // dimension std::optional<wgpu::TextureViewDimension>
      // aspect std::optional<wgpu::TextureAspect>
      // baseMipLevel std::optional<double>
      // mipLevelCount std::optional<double>
      // baseArrayLayer std::optional<double>
      // arrayLayerCount std::optional<double>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureViewDescriptor> arg) {
    throw std::runtime_error("Invalid GPUTextureViewDescriptor::toJSI()");
  }
};

} // namespace margelo