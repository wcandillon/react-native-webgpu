#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUTextureView.h"

namespace rnwgpu {

struct GPURenderPassDepthStencilAttachment {
  std::shared_ptr<GPUTextureView> view;    /* GPUTextureView */
  std::optional<double> depthClearValue;   /* number */
  std::optional<wgpu::> depthLoadOp;       /* GPULoadOp */
  std::optional<wgpu::> depthStoreOp;      /* GPUStoreOp */
  std::optional<bool> depthReadOnly;       /* boolean */
  std::optional<double> stencilClearValue; /* GPUStencilValue */
  std::optional<wgpu::> stencilLoadOp;     /* GPULoadOp */
  std::optional<wgpu::> stencilStoreOp;    /* GPUStoreOp */
  std::optional<bool> stencilReadOnly;     /* boolean */
};

} // namespace rnwgpu