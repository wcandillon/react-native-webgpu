#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUTextureBindingLayout {
  std::optional<wgpu::TextureSampleType> sampleType; // GPUTextureSampleType
  std::optional<wgpu::TextureViewDimension>
      viewDimension;                // GPUTextureViewDimension
  std::optional<bool> multisampled; // boolean
};

static bool conv(wgpu::TextureBindingLayout &out,
                 const std::shared_ptr<GPUTextureBindingLayout> &in) {
  return conv(out.sampleType, in->sampleType) &&
         conv(out.viewDimension, in->viewDimension) &&
         conv(out.multisampled, in->multisampled);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // sampleType std::optional<wgpu::TextureSampleType>
      // viewDimension std::optional<wgpu::TextureViewDimension>
      // multisampled std::optional<bool>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureBindingLayout> arg) {
    throw std::runtime_error("Invalid GPUTextureBindingLayout::toJSI()");
  }
};

} // namespace margelo