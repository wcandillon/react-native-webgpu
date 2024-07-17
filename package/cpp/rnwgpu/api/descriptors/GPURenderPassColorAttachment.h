#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUColorDict.h"
#include "GPUTextureView.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassColorAttachment {
  std::shared_ptr<GPUTextureView> view; // GPUTextureView
  std::optional<double> depthSlice;     // GPUIntegerCoordinate
  std::optional<std::shared_ptr<GPUTextureView>>
      resolveTarget; // GPUTextureView
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUColorDict>>>
      clearValue;        // GPUColor
  wgpu::LoadOp loadOp;   // GPULoadOp
  wgpu::StoreOp storeOp; // GPUStoreOp
};

static bool conv(wgpu::RenderPassColorAttachment &out,
                 GPURenderPassColorAttachment &in) {

  out.loadOp = in.loadOp;
  out.storeOp = in.storeOp;
  return conv(out.view, in.view) && conv(out.depthSlice, in.depthSlice) &&
         conv(out.resolveTarget, in.resolveTarget) &&
         conv(out.clearValue, in.clearValue);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassColorAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // view std::shared_ptr<GPUTextureView>
      // depthSlice std::optional<double>
      // resolveTarget std::optional<std::shared_ptr<GPUTextureView>>
      // clearValue std::optional<std::variant<std::vector<double>,
      // std::shared_ptr<GPUColorDict>>> loadOp wgpu::LoadOp storeOp
      // wgpu::StoreOp
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassColorAttachment> arg) {
    throw std::runtime_error("Invalid GPURenderPassColorAttachment::toJSI()");
  }
};

} // namespace margelo