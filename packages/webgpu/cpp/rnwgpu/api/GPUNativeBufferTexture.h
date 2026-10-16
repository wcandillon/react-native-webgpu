#pragma once

#include <memory>
#include <string>
#include <utility>

#include "webgpu/webgpu_cpp.h"

#include "GPUNativeBufferWrapper.h"
#include "GPUTexture.h"
#include "VideoFrame.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// A native color buffer (an RGBA / R8 / ... AHardwareBuffer, a BGRA IOSurface)
// imported once as a regular texture, with the access window of each use
// explicit: GPUHardwareBufferTexture of Dawn's Android layer. Created by
// GPUDevice.createNativeBufferTexture().
class GPUNativeBufferTexture
    : public GPUNativeBufferWrapper<GPUNativeBufferTexture, GPUTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUNativeBufferTexture";

  GPUNativeBufferTexture(wgpu::Device device, wgpu::SharedTextureMemory memory,
                         std::shared_ptr<GPUTexture> texture,
                         std::shared_ptr<VideoFrame> source, std::string label)
      : GPUNativeBufferWrapper(std::move(device), std::move(memory),
                               texture->get(), texture, std::move(source),
                               std::move(label)) {}

  // The texture that aliases the buffer. Bind it only between beginAccess()
  // and endAccess().
  std::shared_ptr<GPUTexture> getTexture() { return _wrapped; }

  // Releases the texture and the import (see NativeBufferAccess::destroy).
  void destroy() {
    _access.destroy();
    _wrapped->destroy();
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    defineWrapperPrototype(runtime, prototype);
    installGetter(runtime, prototype, "texture",
                  &GPUNativeBufferTexture::getTexture);
    installMethod(runtime, prototype, "destroy",
                  &GPUNativeBufferTexture::destroy);
  }
};

} // namespace rnwgpu
