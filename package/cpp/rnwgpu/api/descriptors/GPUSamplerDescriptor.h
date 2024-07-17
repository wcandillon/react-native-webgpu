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

struct GPUSamplerDescriptor {
  std::optional<wgpu::AddressMode> addressModeU;      // GPUAddressMode
  std::optional<wgpu::AddressMode> addressModeV;      // GPUAddressMode
  std::optional<wgpu::AddressMode> addressModeW;      // GPUAddressMode
  std::optional<wgpu::FilterMode> magFilter;          // GPUFilterMode
  std::optional<wgpu::FilterMode> minFilter;          // GPUFilterMode
  std::optional<wgpu::MipmapFilterMode> mipmapFilter; // GPUMipmapFilterMode
  std::optional<double> lodMinClamp;                  // number
  std::optional<double> lodMaxClamp;                  // number
  std::optional<wgpu::CompareFunction> compare;       // GPUCompareFunction
  std::optional<double> maxAnisotropy;                // number
  std::optional<std::string> label;                   // string
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUSamplerDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUSamplerDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUSamplerDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "addressModeU")) {
        auto prop = value.getProperty(runtime, "addressModeU");
        result->addressModeU =
            JSIConverter::fromJSI<std::optional<wgpu::AddressMode>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "addressModeV")) {
        auto prop = value.getProperty(runtime, "addressModeV");
        result->addressModeV =
            JSIConverter::fromJSI<std::optional<wgpu::AddressMode>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "addressModeW")) {
        auto prop = value.getProperty(runtime, "addressModeW");
        result->addressModeW =
            JSIConverter::fromJSI<std::optional<wgpu::AddressMode>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "magFilter")) {
        auto prop = value.getProperty(runtime, "magFilter");
        result->magFilter =
            JSIConverter::fromJSI<std::optional<wgpu::FilterMode>>(runtime,
                                                                   prop, false);
      }
      if (value.hasProperty(runtime, "minFilter")) {
        auto prop = value.getProperty(runtime, "minFilter");
        result->minFilter =
            JSIConverter::fromJSI<std::optional<wgpu::FilterMode>>(runtime,
                                                                   prop, false);
      }
      if (value.hasProperty(runtime, "mipmapFilter")) {
        auto prop = value.getProperty(runtime, "mipmapFilter");
        result->mipmapFilter =
            JSIConverter::fromJSI<std::optional<wgpu::MipmapFilterMode>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "lodMinClamp")) {
        auto prop = value.getProperty(runtime, "lodMinClamp");
        result->lodMinClamp =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "lodMaxClamp")) {
        auto prop = value.getProperty(runtime, "lodMaxClamp");
        result->lodMaxClamp =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "compare")) {
        auto prop = value.getProperty(runtime, "compare");
        result->compare =
            JSIConverter::fromJSI<std::optional<wgpu::CompareFunction>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "maxAnisotropy")) {
        auto prop = value.getProperty(runtime, "maxAnisotropy");
        result->maxAnisotropy =
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
                          std::shared_ptr<rnwgpu::GPUSamplerDescriptor> arg) {
    throw std::runtime_error("Invalid GPUSamplerDescriptor::toJSI()");
  }
};
} // namespace margelo