#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

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

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<
    std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result =
        std::make_unique<rnwgpu::GPURenderPassDepthStencilAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo