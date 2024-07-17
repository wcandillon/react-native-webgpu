#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUStencilFaceState.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUDepthStencilState {
  wgpu::TextureFormat format;                        // GPUTextureFormat
  std::optional<bool> depthWriteEnabled;             // boolean
  std::optional<wgpu::CompareFunction> depthCompare; // GPUCompareFunction
  std::optional<std::shared_ptr<GPUStencilFaceState>>
      stencilFront; // GPUStencilFaceState
  std::optional<std::shared_ptr<GPUStencilFaceState>>
      stencilBack;                           // GPUStencilFaceState
  std::optional<double> stencilReadMask;     // GPUStencilValue
  std::optional<double> stencilWriteMask;    // GPUStencilValue
  std::optional<double> depthBias;           // GPUDepthBias
  std::optional<double> depthBiasSlopeScale; // number
  std::optional<double> depthBiasClamp;      // number
};

static bool conv(wgpu::DepthStencilState &out,
                 std::shared_ptr<GPUDepthStencilState> &in) {

  out.format = in->format;
  return conv(out.depthWriteEnabled, in->depthWriteEnabled) &&
         conv(out.depthCompare, in->depthCompare) &&
         conv(out.stencilFront, in->stencilFront) &&
         conv(out.stencilBack, in->stencilBack) &&
         conv(out.stencilReadMask, in->stencilReadMask) &&
         conv(out.stencilWriteMask, in->stencilWriteMask) &&
         conv(out.depthBias, in->depthBias) &&
         conv(out.depthBiasSlopeScale, in->depthBiasSlopeScale) &&
         conv(out.depthBiasClamp, in->depthBiasClamp);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDepthStencilState>> {
  static std::shared_ptr<rnwgpu::GPUDepthStencilState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDepthStencilState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // format wgpu::TextureFormat
      // depthWriteEnabled std::optional<bool>
      // depthCompare std::optional<wgpu::CompareFunction>
      // stencilFront std::optional<std::shared_ptr<GPUStencilFaceState>>
      // stencilBack std::optional<std::shared_ptr<GPUStencilFaceState>>
      // stencilReadMask std::optional<double>
      // stencilWriteMask std::optional<double>
      // depthBias std::optional<double>
      // depthBiasSlopeScale std::optional<double>
      // depthBiasClamp std::optional<double>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDepthStencilState> arg) {
    throw std::runtime_error("Invalid GPUDepthStencilState::toJSI()");
  }
};

} // namespace margelo