#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUStencilFaceState.h"

namespace rnwgpu {

struct GPUDepthStencilState {
  wgpu::TextureFormat format; /* GPUTextureFormat */
  std::optional<bool> depthWriteEnabled; /* boolean */
  std::optional<wgpu::> depthCompare; /* GPUCompareFunction */
  std::optional<std::shared_ptr<GPUStencilFaceState>> stencilFront; /* GPUStencilFaceState */
  std::optional<std::shared_ptr<GPUStencilFaceState>> stencilBack; /* GPUStencilFaceState */
  std::optional<double> stencilReadMask; /* GPUStencilValue */
  std::optional<double> stencilWriteMask; /* GPUStencilValue */
  std::optional<double> depthBias; /* GPUDepthBias */
  std::optional<double> depthBiasSlopeScale; /* number */
  std::optional<double> depthBiasClamp; /* number */
};

} // namespace rnwgpu