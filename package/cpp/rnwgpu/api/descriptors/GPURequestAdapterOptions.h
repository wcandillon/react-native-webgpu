#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPURequestAdapterOptions {
  std::optional<wgpu::PowerPreference> powerPreference; /* GPUPowerPreference */
  std::optional<bool> forceFallbackAdapter;             /* boolean */
};

} // namespace rnwgpu