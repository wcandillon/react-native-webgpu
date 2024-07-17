#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUCanvasConfiguration {
  unknown device;                     /* GPUDevice */
  wgpu::TextureFormat format;         /* GPUTextureFormat */
  std::optional<double> usage;        /* GPUTextureUsageFlags */
  std::optional<unknown> viewFormats; /* Iterable<GPUTextureFormat> */
  std::optional<wgpu::definedColorSpace> colorSpace; /* PredefinedColorSpace */
  std::optional<wgpu::CanvasAlphaMode> alphaMode;    /* GPUCanvasAlphaMode */
};

} // namespace rnwgpu