#pragma once

#include <optional>

namespace rnwgpu {

struct GPURequestAdapterOptions {
  std::optional<unknown> powerPreference;   // GPUPowerPreference
  std::optional<bool> forceFallbackAdapter; // boolean
};

} // namespace rnwgpu