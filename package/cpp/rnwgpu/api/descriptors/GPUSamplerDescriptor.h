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
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUSamplerDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo