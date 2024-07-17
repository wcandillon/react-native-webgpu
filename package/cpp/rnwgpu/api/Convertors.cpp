#include "Convertors.h"

#include "GPUBindGroupEntry.h"
#include "GPUBufferBinding.h"
#include "GPUSampler.h"

namespace rnwgpu {

bool conv(wgpu::BindGroupEntry &out, const GPUBindGroupEntry &in) {
  // out = {};
  if (!conv(out.binding, in.binding)) {
    return false;
  }

  if (auto *res = std::get_if<std::shared_ptr<GPUSampler>>(&in.resource)) {
    return conv(out.sampler, *res);
  }
  if (auto *res = std::get_if<std::shared_ptr<GPUTextureView>>(&in.resource)) {
    return conv(out.textureView, *res);
  }
  if (auto *res =
          std::get_if<std::shared_ptr<GPUBufferBinding>>(&in.resource)) {
    auto buffer = &(res->get());
    out.size = wgpu::kWholeSize;
    if (!buffer || !conv(out.offset, res->offset) ||
        !conv(out.size, res->size)) {
      return false;
    }
    out.buffer = *buffer;
    return true;
  }
  if (auto *res =
          std::get_if<std::shared_ptr<GPUExternalTexture>>(&in.resource)) {
    throw std::runtime_error("External textures not supported yet");
  }
  return false;
}

} // namespace rnwgpu