#include "GPUSharedTextureMemory.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "Convertors.h"

namespace rnwgpu {

std::shared_ptr<GPUTexture> GPUSharedTextureMemory::createTexture(
    std::optional<std::shared_ptr<GPUTextureDescriptor>> descriptor) {
  if (!descriptor.has_value() || descriptor.value() == nullptr) {
    auto texture = _instance.CreateTexture();
    return std::make_shared<GPUTexture>(texture, "");
  }

  wgpu::TextureDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor.value())) {
    throw std::runtime_error(
        "GPUSharedTextureMemory::createTexture(): Error with "
        "GPUTextureDescriptor");
  }
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture,
                                      descriptor.value()->label.value_or(""));
}

bool GPUSharedTextureMemory::beginAccess(std::shared_ptr<GPUTexture> texture,
                                         bool initialized) {
  if (!texture) {
    throw std::runtime_error(
        "GPUSharedTextureMemory::beginAccess(): texture is null");
  }
  wgpu::SharedTextureMemoryBeginAccessDescriptor desc{};
  desc.initialized = initialized;
  desc.concurrentRead = false;
  desc.fenceCount = 0;
  desc.fences = nullptr;
  desc.signaledValues = nullptr;
  auto status = _instance.BeginAccess(texture->get(), &desc);
  return static_cast<bool>(status);
}

bool GPUSharedTextureMemory::endAccess(std::shared_ptr<GPUTexture> texture) {
  if (!texture) {
    throw std::runtime_error(
        "GPUSharedTextureMemory::endAccess(): texture is null");
  }
  wgpu::SharedTextureMemoryEndAccessState state{};
  auto status = _instance.EndAccess(texture->get(), &state);
  return static_cast<bool>(status);
}

} // namespace rnwgpu
