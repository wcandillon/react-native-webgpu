#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUDepthStencilState {
  wgpu::TextureFormat format;                        /* GPUTextureFormat */
  std::optional<bool> depthWriteEnabled;             /* boolean */
  std::optional<wgpu::CompareFunction> depthCompare; /* GPUCompareFunction */
  std::optional<unknown> stencilFront;               /* GPUStencilFaceState */
  std::optional<unknown> stencilBack;                /* GPUStencilFaceState */
  std::optional<double> stencilReadMask;             /* GPUStencilValue */
  std::optional<double> stencilWriteMask;            /* GPUStencilValue */
  std::optional<double> depthBias;                   /* GPUDepthBias */
  std::optional<double> depthBiasSlopeScale;         /* number */
  std::optional<double> depthBiasClamp;              /* number */
};

} // namespace rnwgpu