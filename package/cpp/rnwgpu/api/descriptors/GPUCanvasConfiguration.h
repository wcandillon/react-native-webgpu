#pragma once

#include <optional>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUDevice.h"

namespace rnwgpu {

struct GPUCanvasConfiguration {
  std::shared_ptr<GPUDevice> device; /* GPUDevice */
  wgpu::TextureFormat format; /* GPUTextureFormat */
  std::optional<double> usage; /* GPUTextureUsageFlags */
  std::optional<std::vector<wgpu::TextureFormat>> viewFormats; /* Iterable<GPUTextureFormat> */
  std::optional<wgpu::> colorSpace; /* PredefinedColorSpace */
  std::optional<wgpu::> alphaMode; /* GPUCanvasAlphaMode */
};

} // namespace rnwgpu