#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUImageCopyExternalImage {
  unknown source;                /* GPUImageCopyExternalImageSource */
  std::optional<unknown> origin; /* GPUOrigin2DStrict */
  std::optional<bool> flipY;     /* boolean */
};

} // namespace rnwgpu