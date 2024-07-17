#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUBufferBindingLayout.h"
#include "GPUExternalTextureBindingLayout.h"
#include "GPUSamplerBindingLayout.h"
#include "GPUStorageTextureBindingLayout.h"
#include "GPUTextureBindingLayout.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBindGroupLayoutEntry {
  double binding;    // GPUIndex32
  double visibility; // GPUShaderStageFlags
  std::optional<std::shared_ptr<GPUBufferBindingLayout>>
      buffer; // GPUBufferBindingLayout
  std::optional<std::shared_ptr<GPUSamplerBindingLayout>>
      sampler; // GPUSamplerBindingLayout
  std::optional<std::shared_ptr<GPUTextureBindingLayout>>
      texture; // GPUTextureBindingLayout
  std::optional<std::shared_ptr<GPUStorageTextureBindingLayout>>
      storageTexture; // GPUStorageTextureBindingLayout
  std::optional<std::shared_ptr<GPUExternalTextureBindingLayout>>
      externalTexture; // GPUExternalTextureBindingLayout
};

static bool conv(wgpu::BindGroupLayoutEntry &out, GPUBindGroupLayoutEntry &in) {

  return conv(out.binding, in.binding) && conv(out.visibility, in.visibility) &&
         conv(out.buffer, in.buffer) && conv(out.sampler, in.sampler) &&
         conv(out.texture, in.texture) &&
         conv(out.storageTexture, in.storageTexture) &&
         conv(out.externalTexture, in.externalTexture);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupLayoutEntry>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // binding double
      // visibility double
      // buffer std::optional<std::shared_ptr<GPUBufferBindingLayout>>
      // sampler std::optional<std::shared_ptr<GPUSamplerBindingLayout>>
      // texture std::optional<std::shared_ptr<GPUTextureBindingLayout>>
      // storageTexture
      // std::optional<std::shared_ptr<GPUStorageTextureBindingLayout>>
      // externalTexture
      // std::optional<std::shared_ptr<GPUExternalTextureBindingLayout>>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUBindGroupLayoutEntry> arg) {
    throw std::runtime_error("Invalid GPUBindGroupLayoutEntry::toJSI()");
  }
};

} // namespace margelo