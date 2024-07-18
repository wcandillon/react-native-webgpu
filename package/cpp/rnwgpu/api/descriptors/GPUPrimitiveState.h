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

struct GPUPrimitiveState {
  std::optional<wgpu::PrimitiveTopology> topology;   // GPUPrimitiveTopology
  std::optional<wgpu::IndexFormat> stripIndexFormat; // GPUIndexFormat
  std::optional<wgpu::FrontFace> frontFace;          // GPUFrontFace
  std::optional<wgpu::CullMode> cullMode;            // GPUCullMode
  std::optional<bool> unclippedDepth;                // boolean
};

static bool conv(wgpu::PrimitiveState &out,
                 const std::shared_ptr<GPUPrimitiveState> &in) {
  if (in->unclippedDepth) {
    // TODO: fix memory leak here
    wgpu::PrimitiveDepthClipControl *depthClip =
        new wgpu::PrimitiveDepthClipControl();
    depthClip->unclippedDepth = true;
    out.nextInChain = depthClip;
  }
  return conv(out.topology, in->topology) &&
         conv(out.stripIndexFormat, in->stripIndexFormat) &&
         conv(out.frontFace, in->frontFace) && conv(out.cullMode, in->cullMode);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPrimitiveState>> {
  static std::shared_ptr<rnwgpu::GPUPrimitiveState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPrimitiveState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // topology std::optional<wgpu::PrimitiveTopology>
      // stripIndexFormat std::optional<wgpu::IndexFormat>
      // frontFace std::optional<wgpu::FrontFace>
      // cullMode std::optional<wgpu::CullMode>
      // unclippedDepth std::optional<bool>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUPrimitiveState> arg) {
    throw std::runtime_error("Invalid GPUPrimitiveState::toJSI()");
  }
};

} // namespace margelo