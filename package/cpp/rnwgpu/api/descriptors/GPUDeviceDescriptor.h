#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUDeviceDescriptor {
  std::optional<unknown> requiredFeatures; // Iterable<GPUFeatureName>
  std::optional<unknown> requiredLimits; // Record<     string,     GPUSize64 >
  std::optional<unknown> defaultQueue;   // GPUQueueDescriptor
  std::optional<std::string> label;      // string
};

} // namespace rnwgpu