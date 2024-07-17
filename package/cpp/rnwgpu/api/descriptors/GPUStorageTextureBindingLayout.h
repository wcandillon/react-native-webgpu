#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUStorageTextureBindingLayout {
  std::optional<wgpu::StorageTextureAccess> access; // GPUStorageTextureAccess
  wgpu::TextureFormat format;                       // GPUTextureFormat
  std::optional<wgpu::TextureViewDimension>
      viewDimension; // GPUTextureViewDimension
};

} // namespace rnwgpu