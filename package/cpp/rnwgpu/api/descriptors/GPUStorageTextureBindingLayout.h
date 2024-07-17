#pragma once

#include <optional>

namespace rnwgpu {

struct GPUStorageTextureBindingLayout {
  std::optional<unknown> access;        // GPUStorageTextureAccess
  unknown format;                       // GPUTextureFormat
  std::optional<unknown> viewDimension; // GPUTextureViewDimension
};

} // namespace rnwgpu