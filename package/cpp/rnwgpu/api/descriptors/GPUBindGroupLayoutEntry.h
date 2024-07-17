#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUBufferBindingLayout.h"
#include "GPUExternalTextureBindingLayout.h"
#include "GPUSamplerBindingLayout.h"
#include "GPUStorageTextureBindingLayout.h"
#include "GPUTextureBindingLayout.h"

namespace rnwgpu {

struct GPUBindGroupLayoutEntry {
  double binding;    // GPUIndex32
  double visibility; // GPUShaderStageFlags
  std::optional<std::shared_ptr<GPUBufferBindingLayout>>
      buffer; // GPUBufferBindingLayout
  std::optional<std::shared_ptr<GPUSamplerBindingLayout>>
      sampler; // GPUSamplerBindingLayout
  std::optional<std::shared_ptr<GPUTextureBindingLayout>>
      texture; // GPUTextureBindingLayout
  std::optional<std::shared_ptr<GPUStorageTextureBindingLayout>>
      storageTexture; // GPUStorageTextureBindingLayout
  std::optional<std::shared_ptr<GPUExternalTextureBindingLayout>>
      externalTexture; // GPUExternalTextureBindingLayout
};

} // namespace rnwgpu