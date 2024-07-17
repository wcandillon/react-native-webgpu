#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPURequestAdapterOptions {
  std::optional<wgpu::> powerPreference;    /* GPUPowerPreference */
  std::optional<bool> forceFallbackAdapter; /* boolean */
};

} // namespace rnwgpu