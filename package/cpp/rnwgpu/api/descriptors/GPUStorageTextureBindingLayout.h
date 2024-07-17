#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUStorageTextureBindingLayout {
  std::optional<wgpu::> access;        /* GPUStorageTextureAccess */
  wgpu::TextureFormat format;          /* GPUTextureFormat */
  std::optional<wgpu::> viewDimension; /* GPUTextureViewDimension */
};

} // namespace rnwgpu