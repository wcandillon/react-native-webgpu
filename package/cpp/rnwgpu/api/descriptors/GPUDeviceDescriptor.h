#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUQueueDescriptor.h"

namespace rnwgpu {

struct GPUDeviceDescriptor {
  std::optional<std::vector<wgpu::FeatureName>>
      requiredFeatures; // Iterable<GPUFeatureName>
  std::optional<std::map<std::string, double>>
      requiredLimits; // Record< string, GPUSize64 >
  std::optional<std::shared_ptr<GPUQueueDescriptor>>
      defaultQueue;                 // GPUQueueDescriptor
  std::optional<std::string> label; // string
};

} // namespace rnwgpu