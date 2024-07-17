#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"



namespace rnwgpu {

struct GPUBufferBindingLayout {
  std::optional<wgpu::> type; /* GPUBufferBindingType */
  std::optional<bool> hasDynamicOffset; /* boolean */
  std::optional<double> minBindingSize; /* GPUSize64 */
};

} // namespace rnwgpu