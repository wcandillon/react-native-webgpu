#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"

namespace rnwgpu {

struct GPUExternalTextureDescriptor;

// The steps of importing a native frame (an IOSurface on Apple platforms, an
// AHardwareBuffer on Android) into Dawn, shared by importExternalTexture(),
// importSharedTextureMemory() and the native buffer wrappers of
// createNativeBufferTexture() / createNativeBufferExternalTexture(): the
// import, the access window around each use, and the external texture that
// samples a YUV frame as RGB. The same steps as Dawn's own Android layer
// (tools/android/webgpu, GPUImportedAHB.cpp), so a JS caller can follow its
// import-once, begin/end-per-frame recipe. Each step throws with the cause,
// prefixed with `caller`, the entry point the message names.
namespace native_frame {

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

// The external texture that samples `texture`, created from the buffer of
// `frame`, as RGB: a YUV frame is converted with the color space of the frame
// (overridden by the descriptor's sourceColorSpace), and the descriptor's
// crop, rotation and mirroring apply.
wgpu::ExternalTexture createExternalTexture(
    const wgpu::Device &device, const wgpu::SharedTextureMemory &memory,
    const wgpu::Texture &texture, const VideoFrameHandle &frame,
    const GPUExternalTextureDescriptor &descriptor, const std::string &label,
    const char *caller);

// The fences of an end-access state as one: on Android their sync fds are
// merged into a single one (what Dawn's Android layer hands back from
// endAccess), on Apple platforms the device's shared event at the largest
// value. False when the state has no fence (or, on Android, only signaled
// ones). Throws if the fences cannot be merged into one: Android fences that
// are not sync fds, Apple fences on different shared events.
bool mergeFences(const wgpu::Device &device,
                 const wgpu::SharedTextureMemoryEndAccessState &state,
                 wgpu::SharedFence *fence, uint64_t *signaledValue,
                 const char *caller);

} // namespace native_frame

} // namespace rnwgpu
