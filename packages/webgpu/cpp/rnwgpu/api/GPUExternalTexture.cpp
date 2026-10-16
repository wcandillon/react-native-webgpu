#include "GPUExternalTexture.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "GPUExternalTextureDescriptor.h"
#include "NativeFrameImport.h"

namespace rnwgpu {

std::shared_ptr<GPUExternalTexture> GPUExternalTexture::Create(
    wgpu::Device device,
    std::shared_ptr<GPUExternalTextureDescriptor> descriptor) {
  static constexpr const char *kCaller = "GPUDevice::importExternalTexture";
  if (!descriptor || !descriptor->source) {
    throw std::runtime_error(
        "GPUDevice::importExternalTexture(): descriptor.source "
        "(NativeVideoFrame) is required");
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
        device, memory, texture, frame, *descriptor, label, kCaller);
  } catch (...) {
    native_frame::endAccess(memory, texture);
    throw;
  }

  return std::make_shared<GPUExternalTexture>(
      std::move(external), std::move(memory), std::move(texture),
      std::move(descriptor->source), std::move(label));
}

} // namespace rnwgpu
