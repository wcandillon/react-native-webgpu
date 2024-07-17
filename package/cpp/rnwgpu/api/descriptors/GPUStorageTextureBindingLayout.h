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

struct GPUStorageTextureBindingLayout {
  std::optional<wgpu::StorageTextureAccess> access; // GPUStorageTextureAccess
  wgpu::TextureFormat format;                       // GPUTextureFormat
  std::optional<wgpu::TextureViewDimension>
      viewDimension; // GPUTextureViewDimension
};

bool conv(wgpu::StorageTextureBindingLayout &out,
          const GPUStorageTextureBindingLayout &in) {

  out.format = in.format;
  return conv(out.access, in.access) &&
         conv(out.viewDimension, in.viewDimension);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUStorageTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // access std::optional<wgpu::StorageTextureAccess>
      // format wgpu::TextureFormat
      // viewDimension std::optional<wgpu::TextureViewDimension>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUStorageTextureBindingLayout> arg) {
    throw std::runtime_error("Invalid GPUStorageTextureBindingLayout::toJSI()");
  }
};

} // namespace margelo