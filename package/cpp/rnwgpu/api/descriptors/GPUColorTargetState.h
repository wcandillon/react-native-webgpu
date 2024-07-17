#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUBlendState.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUColorTargetState {
  wgpu::TextureFormat format;                          // GPUTextureFormat
  std::optional<std::shared_ptr<GPUBlendState>> blend; // GPUBlendState
  std::optional<double> writeMask;                     // GPUColorWriteFlags
};

static bool conv(wgpu::ColorTargetState &out,
                 std::shared_ptr<GPUColorTargetState> &in) {

  out.format = in->format;
  return conv(out.blend, in->blend) && conv(out.writeMask, in->writeMask);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorTargetState>> {
  static std::shared_ptr<rnwgpu::GPUColorTargetState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUColorTargetState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // format wgpu::TextureFormat
      // blend std::optional<std::shared_ptr<GPUBlendState>>
      // writeMask std::optional<double>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUColorTargetState> arg) {
    throw std::runtime_error("Invalid GPUColorTargetState::toJSI()");
  }
};

} // namespace margelo