#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassLayout {
  std::vector<std::variant<wgpu::TextureFormat, std::nullptr_t>>
      colorFormats; // Iterable<GPUTextureFormat | null>
  std::optional<wgpu::TextureFormat> depthStencilFormat; // GPUTextureFormat
  std::optional<double> sampleCount;                     // GPUSize32
  std::optional<std::string> label;                      // string
};

static bool conv(wgpu::RenderPassLayout &out,
                 const std::shared_ptr<GPURenderPassLayout> &in) {

  return conv(out.colorFormats, in->colorFormats) &&
         conv(out.depthStencilFormat, in->depthStencilFormat) &&
         conv(out.sampleCount, in->sampleCount) && conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassLayout>> {
  static std::shared_ptr<rnwgpu::GPURenderPassLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // colorFormats std::vector<std::variant<wgpu::TextureFormat,
      // std::nullptr_t>>
      // depthStencilFormat std::optional<wgpu::TextureFormat>
      // sampleCount std::optional<double>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPURenderPassLayout> arg) {
    throw std::runtime_error("Invalid GPURenderPassLayout::toJSI()");
  }
};

} // namespace margelo