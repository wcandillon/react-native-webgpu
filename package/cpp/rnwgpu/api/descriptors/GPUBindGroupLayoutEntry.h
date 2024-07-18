#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
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

static bool conv(wgpu::BindGroupLayoutEntry &out,
                 const std::shared_ptr<GPUBindGroupLayoutEntry> &in) {
  return conv(out.binding, in->binding) &&
         conv(out.visibility, in->visibility) && conv(out.buffer, in->buffer) &&
         conv(out.sampler, in->sampler) && conv(out.texture, in->texture) &&
         conv(out.storageTexture, in->storageTexture) &&
         conv(out.externalTexture, in->externalTexture);
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
      if (value.hasProperty(runtime, "binding")) {
        auto prop = value.getProperty(runtime, "binding");
        result->binding = JSIConverter<double>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "visibility")) {
        auto prop = value.getProperty(runtime, "visibility");
        result->visibility =
            JSIConverter<double>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "buffer")) {
        auto prop = value.getProperty(runtime, "buffer");
        if (!prop.isUndefined()) {
          result->buffer = JSIConverter<std::optional<
              std::shared_ptr<GPUBufferBindingLayout>>>::fromJSI(runtime, prop,
                                                                 false);
        }
      }
      if (value.hasProperty(runtime, "sampler")) {
        auto prop = value.getProperty(runtime, "sampler");
        if (!prop.isUndefined()) {
          result->sampler = JSIConverter<std::optional<
              std::shared_ptr<GPUSamplerBindingLayout>>>::fromJSI(runtime, prop,
                                                                  false);
        }
      }
      if (value.hasProperty(runtime, "texture")) {
        auto prop = value.getProperty(runtime, "texture");
        if (!prop.isUndefined()) {
          result->texture = JSIConverter<std::optional<
              std::shared_ptr<GPUTextureBindingLayout>>>::fromJSI(runtime, prop,
                                                                  false);
        }
      }
      if (value.hasProperty(runtime, "storageTexture")) {
        auto prop = value.getProperty(runtime, "storageTexture");
        if (!prop.isUndefined()) {
          result->storageTexture = JSIConverter<std::optional<std::shared_ptr<
              GPUStorageTextureBindingLayout>>>::fromJSI(runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "externalTexture")) {
        auto prop = value.getProperty(runtime, "externalTexture");
        if (!prop.isUndefined()) {
          result->externalTexture = JSIConverter<std::optional<std::shared_ptr<
              GPUExternalTextureBindingLayout>>>::fromJSI(runtime, prop, false);
        }
      }
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