#pragma once

#include <optional>

namespace rnwgpu {

struct GPUCanvasConfiguration {
  unknown device;                     // GPUDevice
  unknown format;                     // GPUTextureFormat
  std::optional<double> usage;        // GPUTextureUsageFlags
  std::optional<unknown> viewFormats; // Iterable<GPUTextureFormat>
  std::optional<unknown> colorSpace;  // PredefinedColorSpace
  std::optional<unknown> alphaMode;   // GPUCanvasAlphaMode
};

} // namespace rnwgpu