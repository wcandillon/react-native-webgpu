#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"

namespace rnwgpu {

struct GPUExternalTextureDescriptor;

// The steps of importing a native buffer (an IOSurface on Apple platforms, an
// AHardwareBuffer on Android) into Dawn, shared by importExternalTexture(),
// importSharedTextureMemory() and copyExternalImageToTexture(): the import,
// the access window around each use, and the external texture that samples a
// YUV buffer as RGB. Each step throws with the cause, prefixed with `caller`,
// the entry point the message names.
namespace native_frame {

// What a texture created from a GPUSharedTextureMemory remembers of its
// import, so importExternalTexture({ source: texture }) can build an external
// texture over it: the memory (the YCbCr properties of an Android buffer) and
// the color space the producer reported for the buffer, if any.
struct SharedTextureOrigin {
  wgpu::SharedTextureMemory memory;
  VideoFrameColorSpace colorSpace;
};

// Import the frame's buffer as shared texture memory.
wgpu::SharedTextureMemory importMemory(const wgpu::Device &device,
                                       const VideoFrameHandle &frame,
                                       const std::string &label,
                                       const char *caller);

// Begin an access to `texture`: Dawn's GPU work on it waits for each fence to
// reach its signaled value first. `initialized` declares whether the buffer
// holds pixels to keep (true for a frame a producer wrote). Exclusive access.
// On Vulkan an initialized image is acquired in the GENERAL layout (an
// UNDEFINED one lets the driver discard the frame on the transition), an
// uninitialized one in UNDEFINED.
void beginAccess(const wgpu::SharedTextureMemory &memory,
                 const wgpu::Texture &texture, bool initialized,
                 const std::vector<wgpu::SharedFence> &fences,
                 const std::vector<uint64_t> &signaledValues,
                 const char *caller);

// End the access. `state` receives the fences Dawn produced: each signals once
// the GPU work of the access is done.
void endAccess(const wgpu::SharedTextureMemory &memory,
               const wgpu::Texture &texture,
               wgpu::SharedTextureMemoryEndAccessState *state,
               const char *caller);

// End the access and drop its fences, for the paths that release the import
// right after: Dawn keeps the texture alive for in-flight GPU work on its own.
void endAccess(const wgpu::SharedTextureMemory &memory,
               const wgpu::Texture &texture);

// The acquire fence of the frame as a shared fence (Android: the sync fd its
// producer signals once it has written the buffer), or null when the frame
// has none. On a device without shared-fence-sync-fd the fence is waited for
// on the CPU instead, and null is returned.
wgpu::SharedFence importAcquireFence(const wgpu::Device &device,
                                     const VideoFrameHandle &frame);

// The external texture that samples `texture`, created from `memory`, as
// RGB: a YUV buffer is converted with `colorSpace`, what its producer
// reported (overridden by the descriptor's sourceColorSpace), and the
// descriptor's crop, rotation and mirroring apply. No access begins here.
wgpu::ExternalTexture createExternalTexture(
    const wgpu::Device &device, const wgpu::SharedTextureMemory &memory,
    const wgpu::Texture &texture, const VideoFrameColorSpace &colorSpace,
    const GPUExternalTextureDescriptor &descriptor, const std::string &label,
    const char *caller);

// Fences as one: on Android their sync fds are merged into a single one, on
// Apple platforms the shared event at the largest value. False when there is
// no fence (or, on Android, only signaled ones). Throws if the fences cannot
// be merged into one: Android fences that are not sync fds, Apple fences on
// different shared events.
bool mergeFences(const wgpu::Device &device,
                 const std::vector<wgpu::SharedFence> &fences,
                 const std::vector<uint64_t> &signaledValues,
                 wgpu::SharedFence *fence, uint64_t *signaledValue,
                 const char *caller);

} // namespace native_frame

} // namespace rnwgpu
