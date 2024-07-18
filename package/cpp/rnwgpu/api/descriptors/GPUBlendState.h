#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUBlendComponent.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBlendState {
  std::shared_ptr<GPUBlendComponent> color; // GPUBlendComponent
  std::shared_ptr<GPUBlendComponent> alpha; // GPUBlendComponent
};

static bool conv(wgpu::BlendState &out,
                 const std::shared_ptr<GPUBlendState> &in) {

  return conv(out.color, in->color) && conv(out.alpha, in->alpha);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendState>> {
  static std::shared_ptr<rnwgpu::GPUBlendState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBlendState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // color std::shared_ptr<GPUBlendComponent>
      // alpha std::shared_ptr<GPUBlendComponent>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendState> arg) {
    throw std::runtime_error("Invalid GPUBlendState::toJSI()");
  }
};

} // namespace margelo