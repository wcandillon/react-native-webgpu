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

struct GPUBlendComponent {
  std::optional<wgpu::BlendOperation> operation; // GPUBlendOperation
  std::optional<wgpu::BlendFactor> srcFactor;    // GPUBlendFactor
  std::optional<wgpu::BlendFactor> dstFactor;    // GPUBlendFactor
};

bool conv(wgpu::BlendComponent &out, GPUBlendComponent &in) {

  return conv(out.operation, in.operation) &&
         conv(out.srcFactor, in.srcFactor) && conv(out.dstFactor, in.dstFactor);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendComponent>> {
  static std::shared_ptr<rnwgpu::GPUBlendComponent>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBlendComponent>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // operation std::optional<wgpu::BlendOperation>
      // srcFactor std::optional<wgpu::BlendFactor>
      // dstFactor std::optional<wgpu::BlendFactor>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendComponent> arg) {
    throw std::runtime_error("Invalid GPUBlendComponent::toJSI()");
  }
};

} // namespace margelo