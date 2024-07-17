#pragma once

#include <optional>

namespace rnwgpu {

struct GPUBufferBindingLayout {
  std::optional<unknown> type;          // GPUBufferBindingType
  std::optional<bool> hasDynamicOffset; // boolean
  std::optional<double> minBindingSize; // GPUSize64
};

} // namespace rnwgpu