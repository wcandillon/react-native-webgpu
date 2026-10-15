#pragma once

#include <memory>
#include <string>
#include <utility>

#include "NativeFrameImport.h"
#include "NativeObject.h"
#include "VideoFrame.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct GPUExternalTextureDescriptor;

class GPUExternalTexture : public NativeObject<GPUExternalTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUExternalTexture";

  // importExternalTexture(): imports the frame of descriptor.source as shared
  // texture memory, begins an access to it, and wraps the wgpu::ExternalTexture
  // that samples it together with the import it owns. The matching EndAccess
  // runs in destroy() / the destructor. Defined in GPUExternalTexture.cpp.
  static std::shared_ptr<GPUExternalTexture>
  Create(wgpu::Device device,
         std::shared_ptr<GPUExternalTextureDescriptor> descriptor);

  // `memory` and `texture` are the import this external texture owns:
  // destroy() ends their access and releases them, so the producer (e.g. an
  // AVPlayer) can reclaim the surface. A GPUHardwareBufferExternalTexture
  // passes null for both: it owns the import itself and brackets each use with
  // its own beginAccess() / endAccess(), so destroy() here only releases the
  // external texture.
  GPUExternalTexture(wgpu::ExternalTexture instance,
                     wgpu::SharedTextureMemory memory, wgpu::Texture texture,
                     std::shared_ptr<VideoFrame> source, std::string label)
      : NativeObject(CLASS_NAME), _instance(std::move(instance)),
        _memory(std::move(memory)), _texture(std::move(texture)),
        _source(std::move(source)), _label(std::move(label)) {}

  ~GPUExternalTexture() override { destroy(); }

public:
  std::string getBrand() { return CLASS_NAME; }

  // End the shared-memory access window and release the underlying resources.
  // Idempotent: safe to call more than once, and the destructor calls it as a
  // garbage-collection fallback. Call it right after the queue.submit() that
  // sampled this texture (never before): a GPUExternalTexture's access window
  // is owned by this wrapper's lifetime, not by submit, so without an explicit
  // destroy() the producer's surface (e.g. an AVPlayer IOSurface) stays claimed
  // until GC runs. EndAccess is the designed post-submit call: Dawn keeps the
  // texture alive for in-flight GPU work via the fences it returns.
  // Destroying the texture and the external texture is what actually frees
  // the import: the ExternalTexture's views keep the imported VkImage /
  // IOSurface texture alive until the object itself is destroyed, which
  // otherwise only happens once the JS wrapper (and any bind group that
  // sampled it) is garbage-collected.
  void destroy() {
    if (_memory && _texture) {
      native_frame::endAccess(_memory, _texture);
    }
    if (_texture) {
      _texture.Destroy();
    }
    if (_instance) {
      _instance.Destroy();
    }
    _texture = nullptr;
    _memory = nullptr;
    _instance = nullptr;
    _source = nullptr;
  }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    if (_instance) {
      _instance.SetLabel(_label.c_str());
    }
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUExternalTexture::getBrand);
    installGetterSetter(runtime, prototype, "label",
                        &GPUExternalTexture::getLabel,
                        &GPUExternalTexture::setLabel);
    installMethod(runtime, prototype, "destroy", &GPUExternalTexture::destroy);
  }

  inline const wgpu::ExternalTexture get() { return _instance; }

private:
  wgpu::ExternalTexture _instance;
  wgpu::SharedTextureMemory _memory;
  wgpu::Texture _texture;
  std::shared_ptr<VideoFrame> _source;
  std::string _label;
};

} // namespace rnwgpu
