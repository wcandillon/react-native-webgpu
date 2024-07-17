#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUUncapturedErrorEventInit {
  unknown error;                  /* GPUError */
  std::optional<bool> bubbles;    /* boolean */
  std::optional<bool> cancelable; /* boolean */
  std::optional<bool> composed;   /* boolean */
};

} // namespace rnwgpu