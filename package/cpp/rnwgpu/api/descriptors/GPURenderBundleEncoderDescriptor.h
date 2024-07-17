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

struct GPURenderBundleEncoderDescriptor {
  std::optional<bool> depthReadOnly;   // boolean
  std::optional<bool> stencilReadOnly; // boolean
  std::vector<std::variant<wgpu::TextureFormat, std::nullptr_t>>
      colorFormats; // Iterable<GPUTextureFormat | null>
  std::optional<wgpu::TextureFormat> depthStencilFormat; // GPUTextureFormat
  std::optional<double> sampleCount;                     // GPUSize32
  std::optional<std::string> label;                      // string
};

bool conv(wgpu::RenderBundleEncoderDescriptor &out,
          const GPURenderBundleEncoderDescriptor &in) {
  out.colorFormatCount = in.colorFormats.size();

  return conv(out.depthReadOnly, in.depthReadOnly) &&
         conv(out.stencilReadOnly, in.stencilReadOnly) &&
         conv(out.colorFormats, in.colorFormats) &&
         conv(out.depthStencilFormat, in.depthStencilFormat) &&
         conv(out.sampleCount, in.sampleCount) && conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderBundleEncoderDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depthReadOnly")) {
        auto prop = value.getProperty(runtime, "depthReadOnly");
        result->depthReadOnly =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto prop = value.getProperty(runtime, "stencilReadOnly");
        result->stencilReadOnly =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "colorFormats")) {
        auto prop = value.getProperty(runtime, "colorFormats");
        result->colorFormats = JSIConverter<
            std::vector<std::variant<wgpu::TextureFormat, std::nullptr_t>>>::
            fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthStencilFormat")) {
        auto prop = value.getProperty(runtime, "depthStencilFormat");
        result->depthStencilFormat =
            JSIConverter<std::optional<wgpu::TextureFormat>>::fromJSI(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto prop = value.getProperty(runtime, "sampleCount");
        result->sampleCount =
            JSIConverter<std::optional<double>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter<std::optional<std::string>>::fromJSI(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor> arg) {
    throw std::runtime_error(
        "Invalid GPURenderBundleEncoderDescriptor::toJSI()");
  }
};

} // namespace margelo