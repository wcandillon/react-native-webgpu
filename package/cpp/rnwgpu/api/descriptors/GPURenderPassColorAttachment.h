#pragma once

#include <optional>

namespace rnwgpu {

struct GPURenderPassColorAttachment {
  unknown view;                         // GPUTextureView
  std::optional<double> depthSlice;     // GPUIntegerCoordinate
  std::optional<unknown> resolveTarget; // GPUTextureView
  std::optional<unknown> clearValue;    // GPUColor
  unknown loadOp;                       // GPULoadOp
  unknown storeOp;                      // GPUStoreOp
};

} // namespace rnwgpu