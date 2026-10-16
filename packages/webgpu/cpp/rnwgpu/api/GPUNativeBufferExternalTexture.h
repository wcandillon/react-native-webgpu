#pragma once

#include <memory>
#include <string>
#include <utility>

#include "NativeObject.h"

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
    : public NativeObject<GPUNativeBufferExternalTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUNativeBufferExternalTexture";

  GPUNativeBufferExternalTexture(
      wgpu::Device device, wgpu::SharedTextureMemory memory,
      wgpu::Texture texture,
      std::shared_ptr<GPUExternalTexture> externalTexture,
      std::shared_ptr<VideoFrame> source, std::string label)
      : NativeObject(CLASS_NAME),
        _access(CLASS_NAME, std::move(device), std::move(memory),
                std::move(texture), std::move(source), std::move(label)),
        _externalTexture(std::move(externalTexture)) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  // The external texture to bind (a texture_external binding). Sample it only
  // between beginAccess() and endAccess().
  std::shared_ptr<GPUExternalTexture> getExternalTexture() {
    return _externalTexture;
  }

  jsi::Value beginAccess(jsi::Runtime &runtime, const jsi::Value &,
                         const jsi::Value *args, size_t count) {
    _access.beginAccess(runtime, args, count);
    return jsi::Value::undefined();
  }

  jsi::Value endAccess(jsi::Runtime &runtime, const jsi::Value &,
                       const jsi::Value *, size_t) {
    return _access.endAccess(runtime);
  }

  // Releases the external texture and the import (see
  // NativeBufferAccess::destroy), the order of Dawn's layer.
  void destroy() {
    _externalTexture->destroy();
    _access.destroy();
  }

  std::string getLabel() { return _access.label(); }
  void setLabel(const std::string &label) {
    _access.setLabel(label);
    _externalTexture->setLabel(label);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand",
                  &GPUNativeBufferExternalTexture::getBrand);
    installGetter(runtime, prototype, "externalTexture",
                  &GPUNativeBufferExternalTexture::getExternalTexture);
    installMethod(runtime, prototype, "beginAccess",
                  &GPUNativeBufferExternalTexture::beginAccess);
    installMethod(runtime, prototype, "endAccess",
                  &GPUNativeBufferExternalTexture::endAccess);
    installMethod(runtime, prototype, "destroy",
                  &GPUNativeBufferExternalTexture::destroy);
    installGetterSetter(runtime, prototype, "label",
                        &GPUNativeBufferExternalTexture::getLabel,
                        &GPUNativeBufferExternalTexture::setLabel);
  }

private:
  NativeBufferAccess _access;
  std::shared_ptr<GPUExternalTexture> _externalTexture;
};

} // namespace rnwgpu
