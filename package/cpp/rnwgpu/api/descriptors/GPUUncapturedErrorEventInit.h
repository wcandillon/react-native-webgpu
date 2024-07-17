#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUError.h"

namespace rnwgpu {

struct GPUUncapturedErrorEventInit {
  std::shared_ptr<GPUError> error; /* GPUError */
  std::optional<bool> bubbles;     /* boolean */
  std::optional<bool> cancelable;  /* boolean */
  std::optional<bool> composed;    /* boolean */
};

} // namespace rnwgpu