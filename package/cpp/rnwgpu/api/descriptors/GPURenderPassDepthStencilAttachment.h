#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUTextureView.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassDepthStencilAttachment {
  std::shared_ptr<GPUTextureView> view;        // GPUTextureView
  std::optional<double> depthClearValue;       // number
  std::optional<wgpu::LoadOp> depthLoadOp;     // GPULoadOp
  std::optional<wgpu::StoreOp> depthStoreOp;   // GPUStoreOp
  std::optional<bool> depthReadOnly;           // boolean
  std::optional<double> stencilClearValue;     // GPUStencilValue
  std::optional<wgpu::LoadOp> stencilLoadOp;   // GPULoadOp
  std::optional<wgpu::StoreOp> stencilStoreOp; // GPUStoreOp
  std::optional<bool> stencilReadOnly;         // boolean
};

static bool
conv(wgpu::RenderPassDepthStencilAttachment &out,
     const std::shared_ptr<GPURenderPassDepthStencilAttachment> &in) {
  return conv(out.view, in->view) &&
         conv(out.depthClearValue, in->depthClearValue) &&
         conv(out.depthLoadOp, in->depthLoadOp) &&
         conv(out.depthStoreOp, in->depthStoreOp) &&
         conv(out.depthReadOnly, in->depthReadOnly) &&
         conv(out.stencilClearValue, in->stencilClearValue) &&
         conv(out.stencilLoadOp, in->stencilLoadOp) &&
         conv(out.stencilStoreOp, in->stencilStoreOp) &&
         conv(out.stencilReadOnly, in->stencilReadOnly);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<
    std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result =
        std::make_unique<rnwgpu::GPURenderPassDepthStencilAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // view std::shared_ptr<GPUTextureView>
      // depthClearValue std::optional<double>
      // depthLoadOp std::optional<wgpu::LoadOp>
      // depthStoreOp std::optional<wgpu::StoreOp>
      // depthReadOnly std::optional<bool>
      // stencilClearValue std::optional<double>
      // stencilLoadOp std::optional<wgpu::LoadOp>
      // stencilStoreOp std::optional<wgpu::StoreOp>
      // stencilReadOnly std::optional<bool>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment> arg) {
    throw std::runtime_error(
        "Invalid GPURenderPassDepthStencilAttachment::toJSI()");
  }
};

} // namespace margelo