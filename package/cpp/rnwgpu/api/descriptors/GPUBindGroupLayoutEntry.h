#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUBindGroupLayoutEntry {
  double binding;                         /* GPUIndex32 */
  double visibility;                      /* GPUShaderStageFlags */
  std::optional<unknown> buffer;          /* GPUBufferBindingLayout */
  std::optional<unknown> sampler;         /* GPUSamplerBindingLayout */
  std::optional<unknown> texture;         /* GPUTextureBindingLayout */
  std::optional<unknown> storageTexture;  /* GPUStorageTextureBindingLayout */
  std::optional<unknown> externalTexture; /* GPUExternalTextureBindingLayout */
};

} // namespace rnwgpu