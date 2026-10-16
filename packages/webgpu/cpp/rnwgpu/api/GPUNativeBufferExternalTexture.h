#pragma once

#include <memory>
#include <string>
#include <utility>

#include "webgpu/webgpu_cpp.h"

#include "GPUExternalTexture.h"
#include "GPUNativeBufferWrapper.h"
#include "VideoFrame.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// A native frame buffer (the YUV AHardwareBuffer of a decoder or a camera, an
// NV12 or BGRA IOSurface) imported once as an external texture, with the
// access window of each use explicit: GPUHardwareBufferExternalTexture of
// Dawn's Android layer. Created by
// GPUDevice.createNativeBufferExternalTexture(). Unlike
// importExternalTexture(), which imports the buffer and begins the access on
// every call, the external texture here is built once per buffer, color
// conversion included, and only beginAccess() / endAccess() run per frame.
class GPUNativeBufferExternalTexture
    : public GPUNativeBufferWrapper<GPUNativeBufferExternalTexture,
                                    GPUExternalTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUNativeBufferExternalTexture";

  GPUNativeBufferExternalTexture(
      wgpu::Device device, wgpu::SharedTextureMemory memory,
      wgpu::Texture texture,
      std::shared_ptr<GPUExternalTexture> externalTexture,
      std::shared_ptr<VideoFrame> source, std::string label)
      : GPUNativeBufferWrapper(std::move(device), std::move(memory),
                               std::move(texture), std::move(externalTexture),
                               std::move(source), std::move(label)) {}

  // The external texture to bind (a texture_external binding). Sample it only
  // between beginAccess() and endAccess().
  std::shared_ptr<GPUExternalTexture> getExternalTexture() { return _wrapped; }

  // Releases the external texture and the import (see
  // NativeBufferAccess::destroy), the order of Dawn's layer.
  void destroy() {
    _wrapped->destroy();
    _access.destroy();
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    defineWrapperPrototype(runtime, prototype);
    installGetter(runtime, prototype, "externalTexture",
                  &GPUNativeBufferExternalTexture::getExternalTexture);
    installMethod(runtime, prototype, "destroy",
                  &GPUNativeBufferExternalTexture::destroy);
  }
};

} // namespace rnwgpu
