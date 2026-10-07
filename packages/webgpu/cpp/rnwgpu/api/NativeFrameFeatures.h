#pragma once

#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// The Dawn features behind native frame import on the current platform:
// importSharedTextureMemory, importExternalTexture and
// copyExternalImageToTexture with a NativeVideoFrame. Like
// importExternalTexture on the web, the capability needs no feature request:
// requestDevice() enables each of these the adapter supports (see
// GPUAdapter::requestDevice). A device created by another library and wrapped
// with importDevice() has to request them itself for its frames to import.
inline std::vector<wgpu::FeatureName> nativeFrameImportFeatures() {
#if defined(__APPLE__)
  return {
      // IOSurface import, and the MTLSharedEvent fence that EndAccess exports
      wgpu::FeatureName::SharedTextureMemoryIOSurface,
      wgpu::FeatureName::SharedFenceMTLSharedEvent,
      // The biplanar NV12 layout of camera frames
      wgpu::FeatureName::DawnMultiPlanarFormats,
  };
#elif defined(__ANDROID__)
  return {
      // AHardwareBuffer import, and the sync-fd fence that EndAccess exports
      wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer,
      wgpu::FeatureName::SharedFenceSyncFD,
      // The YUV buffers of the video decoder and of the camera: Dawn imports
      // them as OpaqueYCbCrAndroid textures, a format it only accepts, and
      // only samples as an external texture, with this feature
      wgpu::FeatureName::OpaqueYCbCrAndroidForExternalTexture,
  };
#else
  return {};
#endif
}

} // namespace rnwgpu
