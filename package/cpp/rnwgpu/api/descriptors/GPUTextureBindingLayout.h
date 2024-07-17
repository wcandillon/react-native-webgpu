#pragma once

#include <optional>

namespace rnwgpu {

struct GPUTextureBindingLayout {
  std::optional<unknown> sampleType;    // GPUTextureSampleType
  std::optional<unknown> viewDimension; // GPUTextureViewDimension
  std::optional<bool> multisampled;     // boolean
};

} // namespace rnwgpu