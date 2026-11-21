#include "GPUExternalTexture.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "DestroyedError.h"
#include "GPUExternalTextureDescriptor.h"
#include "NativeFrameImport.h"

namespace rnwgpu {

std::shared_ptr<GPUExternalTexture> GPUExternalTexture::Create(
    wgpu::Device device,
    std::shared_ptr<GPUExternalTextureDescriptor> descriptor) {
  static constexpr const char *kCaller = "GPUDevice::importExternalTexture";
  if (descriptor && descriptor->sourceTexture) {
    // A texture of a GPUSharedTextureMemory: the caller imported the buffer
    // once and brackets each use with the memory's beginAccess() / endAccess(),
    // so the external texture only samples it, and can be kept for every
    // access of the buffer.
    const auto &source = descriptor->sourceTexture;
    const auto &origin = source->sharedTextureOrigin();
    if (!origin) {
      throw std::runtime_error(
          "GPUDevice::importExternalTexture(): descriptor.source is a "
          "GPUTexture that was not created by "
          "GPUSharedTextureMemory.createTexture()");
    }
    // Copied: destroy() on the memory clears the origin's reference.
    wgpu::SharedTextureMemory memory = origin->memory;
    if (!memory) {
      // Like an access to a destroyed memory: a validation error, then the
      // throw.
      reportDestroyedUse(device, "GPUSharedTextureMemory", origin->label,
                         "importExternalTexture");
      throw std::runtime_error(
          "GPUDevice::importExternalTexture(): descriptor.source is a "
          "texture of a GPUSharedTextureMemory that was destroyed");
    }
    std::string label = descriptor->label.value_or("external-texture");
    auto external = native_frame::createExternalTexture(
        device, memory, source->get(), origin->colorSpace, *descriptor, label,
        kCaller);
    return std::make_shared<GPUExternalTexture>(
        std::move(external), nullptr, nullptr, nullptr, std::move(label));
  }
  if (!descriptor || !descriptor->source) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): descriptor.source (a "
        "NativeBuffer, or a texture of a GPUSharedTextureMemory) is required");
  }
  const auto &frame = descriptor->source->handle();
  std::string label = descriptor->label.value_or("external-texture");

  // 1. Import the buffer as shared texture memory, and create the texture.
  //    No descriptor: Dawn picks the right format (biplanar for an NV12
  //    IOSurface, OpaqueYCbCrAndroid for a YUV AHardwareBuffer, R8 / RGBA8 /
  //    ... for color buffers).
  auto memory = native_frame::importMemory(device, frame, label, kCaller);
  auto texture = memory.CreateTexture();
  if (texture == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): CreateTexture returned null");
  }

  // 2. Begin the access, after the producer's fence when the frame has one.
  //    The matching EndAccess runs when the GPUExternalTexture is destroyed
  //    (explicitly via destroy() or at GC).
  std::vector<wgpu::SharedFence> fences;
  std::vector<uint64_t> signaledValues;
  if (auto acquire = native_frame::importAcquireFence(device, frame)) {
    fences.push_back(std::move(acquire));
    signaledValues.push_back(1);
  }
  native_frame::beginAccess(memory, texture, true, fences, signaledValues,
                            kCaller);

  // 3. The external texture that samples the frame as RGB.
  wgpu::ExternalTexture external;
  try {
    external = native_frame::createExternalTexture(
        device, memory, texture, frame.colorSpace, *descriptor, label, kCaller);
  } catch (...) {
    native_frame::endAccess(memory, texture);
    throw;
  }

  return std::make_shared<GPUExternalTexture>(
      std::move(external), std::move(memory), std::move(texture),
      std::move(descriptor->source), std::move(label));
}

} // namespace rnwgpu
