#pragma once

#include <memory>
#include <string>
#include <utility>

#include "NativeObject.h"

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
class GPUNativeBufferTexture : public NativeObject<GPUNativeBufferTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUNativeBufferTexture";

  GPUNativeBufferTexture(wgpu::Device device, wgpu::SharedTextureMemory memory,
                         std::shared_ptr<GPUTexture> texture,
                         std::shared_ptr<VideoFrame> source, std::string label)
      : NativeObject(CLASS_NAME),
        _access(CLASS_NAME, std::move(device), std::move(memory),
                texture->get(), std::move(source), std::move(label)),
        _texture(std::move(texture)) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  // The texture that aliases the buffer. Bind it only between beginAccess()
  // and endAccess().
  std::shared_ptr<GPUTexture> getTexture() { return _texture; }

  jsi::Value beginAccess(jsi::Runtime &runtime, const jsi::Value &,
                         const jsi::Value *args, size_t count) {
    _access.beginAccess(runtime, args, count);
    return jsi::Value::undefined();
  }

  jsi::Value endAccess(jsi::Runtime &runtime, const jsi::Value &,
                       const jsi::Value *, size_t) {
    return _access.endAccess(runtime);
  }

  // Releases the texture and the import (see NativeBufferAccess::destroy).
  void destroy() {
    _access.destroy();
    _texture->destroy();
  }

  std::string getLabel() { return _access.label(); }
  void setLabel(const std::string &label) { _access.setLabel(label); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand",
                  &GPUNativeBufferTexture::getBrand);
    installGetter(runtime, prototype, "texture",
                  &GPUNativeBufferTexture::getTexture);
    installMethod(runtime, prototype, "beginAccess",
                  &GPUNativeBufferTexture::beginAccess);
    installMethod(runtime, prototype, "endAccess",
                  &GPUNativeBufferTexture::endAccess);
    installMethod(runtime, prototype, "destroy",
                  &GPUNativeBufferTexture::destroy);
    installGetterSetter(runtime, prototype, "label",
                        &GPUNativeBufferTexture::getLabel,
                        &GPUNativeBufferTexture::setLabel);
  }

private:
  NativeBufferAccess _access;
  std::shared_ptr<GPUTexture> _texture;
};

} // namespace rnwgpu
