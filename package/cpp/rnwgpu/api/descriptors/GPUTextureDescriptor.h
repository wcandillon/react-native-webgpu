#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUExtent3D.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUTextureDescriptor {
  GPUExtent3D size;                                // GPUExtent3DStrict
  std::optional<double> mipLevelCount;             // GPUIntegerCoordinate
  std::optional<double> sampleCount;               // GPUSize32
  std::optional<wgpu::TextureDimension> dimension; // GPUTextureDimension
  wgpu::TextureFormat format;                      // GPUTextureFormat
  double usage;                                    // GPUTextureUsageFlags
  std::optional<std::vector<wgpu::TextureFormat>>
      viewFormats;                  // Iterable<GPUTextureFormat>
  std::optional<std::string> label; // string
};

static bool conv(wgpu::TextureDescriptor &out, GPUTextureDescriptor &in) {

  out.format = in.format;
  return conv(out.size, in.size) && conv(out.mipLevelCount, in.mipLevelCount) &&
         conv(out.sampleCount, in.sampleCount) &&
         conv(out.dimension, in.dimension) && conv(out.usage, in.usage) &&
         conv(out.viewFormats, in.viewFormats) && conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // size GPUExtent3D
      // mipLevelCount std::optional<double>
      // sampleCount std::optional<double>
      // dimension std::optional<wgpu::TextureDimension>
      // format wgpu::TextureFormat
      // usage double
      // viewFormats std::optional<std::vector<wgpu::TextureFormat>>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUTextureDescriptor> arg) {
    throw std::runtime_error("Invalid GPUTextureDescriptor::toJSI()");
  }
};

} // namespace margelo