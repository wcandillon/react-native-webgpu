#pragma once

#include <memory>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUBufferBinding.h"
#include "GPUExternalTexture.h"
#include "GPUSampler.h"
#include "GPUTextureView.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBindGroupEntry {
  double binding; // GPUIndex32
  std::variant<std::shared_ptr<GPUSampler>, std::shared_ptr<GPUTextureView>,
               std::shared_ptr<GPUBufferBinding>,
               std::shared_ptr<GPUExternalTexture>>
      resource; // GPUBindingResource
};

bool conv(wgpu::BindGroupEntry &out, const GPUBindGroupEntry &in) {
  // out = {};
  if (!conv(out.binding, in.binding)) {
    return false;
  }

  if (auto *res = std::get_if<std::shared_ptr<GPUSampler>>(&in.resource)) {
    return conv(out.sampler, *res);
  }
  if (auto *res = std::get_if<std::shared_ptr<GPUTextureView>>(&in.resource)) {
    return conv(out.textureView, *res);
  }
  if (auto *res =
          std::get_if<std::shared_ptr<GPUBufferBinding>>(&in.resource)) {
    auto buffer = (*res)->buffer->get();
    out.size = wgpu::kWholeSize;
    if (!buffer || !conv(out.offset, (*res)->offset) ||
        !conv(out.size, (*res)->size)) {
      return false;
    }
    out.buffer = buffer;
    return true;
  }
  if (auto *res =
          std::get_if<std::shared_ptr<GPUExternalTexture>>(&in.resource)) {
    throw std::runtime_error("GPUExternalTexture not supported");
  }
  return false;
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupEntry>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // binding double
      // resource std::variant<std::shared_ptr<GPUSampler>,
      // std::shared_ptr<GPUTextureView>, std::shared_ptr<GPUBufferBinding>,
      // std::shared_ptr<GPUExternalTexture>>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupEntry> arg) {
    throw std::runtime_error("Invalid GPUBindGroupEntry::toJSI()");
  }
};

} // namespace margelo