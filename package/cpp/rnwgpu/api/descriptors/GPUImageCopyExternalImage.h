#pragma once

#include <variant>
#include <vector>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "ImageBitmap.h"
#include "ImageData.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "VideoFrame.h"
#include "HTMLCanvasElement.h"
#include "OffscreenCanvas.h"
#include "GPUOrigin2DDictStrict.h"

namespace rnwgpu {

struct GPUImageCopyExternalImage {
  std::variant<std::shared_ptr<ImageBitmap>, std::shared_ptr<ImageData>, std::shared_ptr<HTMLImageElement>, std::shared_ptr<HTMLVideoElement>, std::shared_ptr<VideoFrame>, std::shared_ptr<HTMLCanvasElement>, std::shared_ptr<OffscreenCanvas>> source; /* GPUImageCopyExternalImageSource */
  std::optional<std::variant<std::vector<double>, std::shared_ptr<GPUOrigin2DDictStrict>>> origin; /* GPUOrigin2DStrict */
  std::optional<bool> flipY; /* boolean */
};

} // namespace rnwgpu