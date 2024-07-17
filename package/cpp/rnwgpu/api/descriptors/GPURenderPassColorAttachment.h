#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPURenderPassColorAttachment {
  unknown view;                         /* GPUTextureView */
  std::optional<double> depthSlice;     /* GPUIntegerCoordinate */
  std::optional<unknown> resolveTarget; /* GPUTextureView */
  std::optional<unknown> clearValue;    /* GPUColor */
  wgpu::LoadOp loadOp;                  /* GPULoadOp */
  wgpu::StoreOp storeOp;                /* GPUStoreOp */
};

} // namespace rnwgpu