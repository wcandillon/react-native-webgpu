#pragma once

#include <optional>

namespace rnwgpu {

struct GPURenderPassDepthStencilAttachment {
  unknown view;                            // GPUTextureView
  std::optional<double> depthClearValue;   // number
  std::optional<unknown> depthLoadOp;      // GPULoadOp
  std::optional<unknown> depthStoreOp;     // GPUStoreOp
  std::optional<bool> depthReadOnly;       // boolean
  std::optional<double> stencilClearValue; // GPUStencilValue
  std::optional<unknown> stencilLoadOp;    // GPULoadOp
  std::optional<unknown> stencilStoreOp;   // GPUStoreOp
  std::optional<bool> stencilReadOnly;     // boolean
};

} // namespace rnwgpu