#pragma once

#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// 'GPUSharedFence "label"', or just the class name when unlabeled.
inline std::string describeDestroyed(const char *className,
                                     const std::string &label) {
  return label.empty() ? std::string(className)
                       : std::string(className) + " \"" + label + "\"";
}

// Reports a use of a destroyed object the way Dawn reports an invalid one: a
// validation error, caught by error scopes or else delivered to the device's
// uncapturederror event.
inline void reportDestroyedUse(wgpu::Device &device, const char *className,
                               const std::string &label, const char *method) {
  auto message = "Destroyed " + describeDestroyed(className, label) +
                 " used in " + method + "().";
  device.InjectError(wgpu::ErrorType::Validation, message.c_str());
}

} // namespace rnwgpu
