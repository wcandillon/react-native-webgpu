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

static bool conv(wgpu::StorageTextureBindingLayout &out,
                 const std::shared_ptr<GPUStorageTextureBindingLayout> &in) {
  return conv(out.access, in->access) && conv(out.format, in->format) &&
         conv(out.viewDimension, in->viewDimension);
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
      if (value.hasProperty(runtime, "access")) {
        auto prop = value.getProperty(runtime, "access");
        if (!prop.isUndefined()) {
          result->access =
              JSIConverter<std::optional<wgpu::StorageTextureAccess>>::fromJSI(
                  runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "format")) {
        auto prop = value.getProperty(runtime, "format");
        result->format =
            JSIConverter<wgpu::TextureFormat>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "viewDimension")) {
        auto prop = value.getProperty(runtime, "viewDimension");
        if (!prop.isUndefined()) {
          result->viewDimension =
              JSIConverter<std::optional<wgpu::TextureViewDimension>>::fromJSI(
                  runtime, prop, false);
        }
      }
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