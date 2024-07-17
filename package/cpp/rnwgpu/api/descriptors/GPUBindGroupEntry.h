#pragma once

#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "GPUSampler.h"
#include "GPUTextureView.h"
#include "GPUBufferBinding.h"
#include "GPUExternalTexture.h"

namespace rnwgpu {

struct GPUBindGroupEntry {
  double binding; /* GPUIndex32 */
  std::variant<std::shared_ptr<GPUSampler>, std::shared_ptr<GPUTextureView>, std::shared_ptr<GPUBufferBinding>, std::shared_ptr<GPUExternalTexture>> resource; /* GPUBindingResource */
};

} // namespace rnwgpu