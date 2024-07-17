#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

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

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassLayout>> {
  static std::shared_ptr<rnwgpu::GPURenderPassLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "colorFormats")) {
        auto prop = value.getProperty(runtime, "colorFormats");
        result->colorFormats = JSIConverter::fromJSI<
            std::vector<std::variant<wgpu::TextureFormat, std::nullptr_t>>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthStencilFormat")) {
        auto prop = value.getProperty(runtime, "depthStencilFormat");
        result->depthStencilFormat =
            JSIConverter::fromJSI<std::optional<wgpu::TextureFormat>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto prop = value.getProperty(runtime, "sampleCount");
        result->sampleCount =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter::fromJSI<std::optional<std::string>>(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPURenderPassLayout> arg) {
    throw std::runtime_error("Invalid GPURenderPassLayout::toJSI()");
  }
};
} // namespace margelo