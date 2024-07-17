#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUExtent3DDictStrict.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUTextureDescriptor {
  std::variant<std::vector<double>, std::shared_ptr<GPUExtent3DDictStrict>>
      size;                                        // GPUExtent3DStrict
  std::optional<double> mipLevelCount;             // GPUIntegerCoordinate
  std::optional<double> sampleCount;               // GPUSize32
  std::optional<wgpu::TextureDimension> dimension; // GPUTextureDimension
  wgpu::TextureFormat format;                      // GPUTextureFormat
  double usage;                                    // GPUTextureUsageFlags
  std::optional<std::vector<wgpu::TextureFormat>>
      viewFormats;                  // Iterable<GPUTextureFormat>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "size")) {
        auto prop = value.getProperty(runtime, "size");
        result->size = JSIConverter::fromJSI<std::variant<
            std::vector<double>, std::shared_ptr<GPUExtent3DDictStrict>>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "mipLevelCount")) {
        auto prop = value.getProperty(runtime, "mipLevelCount");
        result->mipLevelCount =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto prop = value.getProperty(runtime, "sampleCount");
        result->sampleCount =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "dimension")) {
        auto prop = value.getProperty(runtime, "dimension");
        result->dimension =
            JSIConverter::fromJSI<std::optional<wgpu::TextureDimension>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "format")) {
        auto prop = value.getProperty(runtime, "format");
        result->format =
            JSIConverter::fromJSI<wgpu::TextureFormat>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "usage")) {
        auto prop = value.getProperty(runtime, "usage");
        result->usage = JSIConverter::fromJSI<double>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "viewFormats")) {
        auto prop = value.getProperty(runtime, "viewFormats");
        result->viewFormats = JSIConverter::fromJSI<
            std::optional<std::vector<wgpu::TextureFormat>>>(runtime, prop,
                                                             false);
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
                          std::shared_ptr<rnwgpu::GPUTextureDescriptor> arg) {
    throw std::runtime_error("Invalid GPUTextureDescriptor::toJSI()");
  }
};
} // namespace margelo