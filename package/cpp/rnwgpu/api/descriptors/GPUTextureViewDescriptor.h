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
      if (value.hasProperty(runtime, "format")) {
        auto prop = value.getProperty(runtime, "format");
        result->format =
            JSIConverter::fromJSI<std::optional<wgpu::TextureFormat>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "dimension")) {
        auto prop = value.getProperty(runtime, "dimension");
        result->dimension =
            JSIConverter::fromJSI<std::optional<wgpu::TextureViewDimension>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "aspect")) {
        auto prop = value.getProperty(runtime, "aspect");
        result->aspect =
            JSIConverter::fromJSI<std::optional<wgpu::TextureAspect>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "baseMipLevel")) {
        auto prop = value.getProperty(runtime, "baseMipLevel");
        result->baseMipLevel =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "mipLevelCount")) {
        auto prop = value.getProperty(runtime, "mipLevelCount");
        result->mipLevelCount =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "baseArrayLayer")) {
        auto prop = value.getProperty(runtime, "baseArrayLayer");
        result->baseArrayLayer =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "arrayLayerCount")) {
        auto prop = value.getProperty(runtime, "arrayLayerCount");
        result->arrayLayerCount =
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
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureViewDescriptor> arg) {
    throw std::runtime_error("Invalid GPUTextureViewDescriptor::toJSI()");
  }
};
} // namespace margelo