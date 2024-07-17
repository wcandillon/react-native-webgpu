#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUColorDict.h"
#include "GPUTextureView.h"

namespace rnwgpu {

struct GPURenderPassColorAttachment {
  std::shared_ptr<GPUTextureView> view; /* GPUTextureView */
  std::optional<double> depthSlice;     /* GPUIntegerCoordinate */
  std::optional<std::shared_ptr<GPUTextureView>>
      resolveTarget; /* GPUTextureView */
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUColorDict>>>
      clearValue;        /* GPUColor */
  wgpu::LoadOp loadOp;   /* GPULoadOp */
  wgpu::StoreOp storeOp; /* GPUStoreOp */
};

} // namespace rnwgpu