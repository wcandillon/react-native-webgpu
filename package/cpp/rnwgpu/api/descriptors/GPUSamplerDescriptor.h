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

static bool conv(wgpu::SamplerDescriptor &out,
                 const std::shared_ptr<GPUSamplerDescriptor> &in) {

  return conv(out.addressModeU, in->addressModeU) &&
         conv(out.addressModeV, in->addressModeV) &&
         conv(out.addressModeW, in->addressModeW) &&
         conv(out.magFilter, in->magFilter) &&
         conv(out.minFilter, in->minFilter) &&
         conv(out.mipmapFilter, in->mipmapFilter) &&
         conv(out.lodMinClamp, in->lodMinClamp) &&
         conv(out.lodMaxClamp, in->lodMaxClamp) &&
         conv(out.compare, in->compare) &&
         conv(out.maxAnisotropy, in->maxAnisotropy) &&
         conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUSamplerDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUSamplerDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUSamplerDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // addressModeU std::optional<wgpu::AddressMode>
      // addressModeV std::optional<wgpu::AddressMode>
      // addressModeW std::optional<wgpu::AddressMode>
      // magFilter std::optional<wgpu::FilterMode>
      // minFilter std::optional<wgpu::FilterMode>
      // mipmapFilter std::optional<wgpu::MipmapFilterMode>
      // lodMinClamp std::optional<double>
      // lodMaxClamp std::optional<double>
      // compare std::optional<wgpu::CompareFunction>
      // maxAnisotropy std::optional<double>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUSamplerDescriptor> arg) {
    throw std::runtime_error("Invalid GPUSamplerDescriptor::toJSI()");
  }
};

} // namespace margelo