#pragma once

#include <vector>
#include <optional>
#include <map>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUObjectDescriptorBase.h"

namespace rnwgpu {

struct GPUDeviceDescriptor {
  std::optional<std::vector<wgpu::FeatureName>> requiredFeatures; /* Iterable<GPUFeatureName> */
  std::optional<std::map<undefined, undefined>> requiredLimits; /* Record<
    string,
    GPUSize64
  > */
  std::optional<std::shared_ptr<GPUObjectDescriptorBase>> defaultQueue; /* GPUQueueDescriptor */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu