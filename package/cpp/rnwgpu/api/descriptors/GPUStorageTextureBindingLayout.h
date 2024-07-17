#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUStorageTextureBindingLayout {
  std::optional<wgpu::StorageTextureAccess>
      access;                 /* GPUStorageTextureAccess */
  wgpu::TextureFormat format; /* GPUTextureFormat */
  std::optional<wgpu::TextureViewDimension>
      viewDimension; /* GPUTextureViewDimension */
};

} // namespace rnwgpu