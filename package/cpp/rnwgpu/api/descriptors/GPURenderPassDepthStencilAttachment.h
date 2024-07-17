#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPURenderPassDepthStencilAttachment {
  unknown view;                                /* GPUTextureView */
  std::optional<double> depthClearValue;       /* number */
  std::optional<wgpu::LoadOp> depthLoadOp;     /* GPULoadOp */
  std::optional<wgpu::StoreOp> depthStoreOp;   /* GPUStoreOp */
  std::optional<bool> depthReadOnly;           /* boolean */
  std::optional<double> stencilClearValue;     /* GPUStencilValue */
  std::optional<wgpu::LoadOp> stencilLoadOp;   /* GPULoadOp */
  std::optional<wgpu::StoreOp> stencilStoreOp; /* GPUStoreOp */
  std::optional<bool> stencilReadOnly;         /* boolean */
};

} // namespace rnwgpu