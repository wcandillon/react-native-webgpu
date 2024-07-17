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

struct GPUStencilFaceState {
  std::optional<wgpu::CompareFunction> compare;      // GPUCompareFunction
  std::optional<wgpu::StencilOperation> failOp;      // GPUStencilOperation
  std::optional<wgpu::StencilOperation> depthFailOp; // GPUStencilOperation
  std::optional<wgpu::StencilOperation> passOp;      // GPUStencilOperation
};

bool conv(wgpu::StencilFaceState &out, GPUStencilFaceState &in) {

  return conv(out.compare, in.compare) && conv(out.failOp, in.failOp) &&
         conv(out.depthFailOp, in.depthFailOp) && conv(out.passOp, in.passOp);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUStencilFaceState>> {
  static std::shared_ptr<rnwgpu::GPUStencilFaceState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUStencilFaceState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // compare std::optional<wgpu::CompareFunction>
      // failOp std::optional<wgpu::StencilOperation>
      // depthFailOp std::optional<wgpu::StencilOperation>
      // passOp std::optional<wgpu::StencilOperation>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUStencilFaceState> arg) {
    throw std::runtime_error("Invalid GPUStencilFaceState::toJSI()");
  }
};

} // namespace margelo