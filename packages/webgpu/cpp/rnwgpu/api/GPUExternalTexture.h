#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Unions.h"

#include "NativeObject.h"
#include "VideoFrame.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUExternalTexture : public NativeObject<GPUExternalTexture> {
public:
  static constexpr const char *CLASS_NAME = "GPUExternalTexture";

  // Construct from an already-built wgpu::ExternalTexture plus the underlying
  // shared-memory resources we need to keep alive. The wrapper takes ownership
  // of the SharedTextureMemory + Texture and calls EndAccess on destruction so
  // the producer (e.g. AVPlayer) can reclaim the IOSurface.
  GPUExternalTexture(wgpu::ExternalTexture instance,
                     wgpu::SharedTextureMemory memory, wgpu::Texture texture,
                     std::shared_ptr<VideoFrame> source, std::string label)
      : NativeObject(CLASS_NAME), _instance(std::move(instance)),
        _memory(std::move(memory)), _texture(std::move(texture)),
        _source(std::move(source)), _label(std::move(label)) {}

  ~GPUExternalTexture() override {
    if (_memory && _texture) {
      wgpu::SharedTextureMemoryEndAccessState state{};
      (void)_memory.EndAccess(_texture, &state);
    }
  }

public:
  std::string getBrand() { return CLASS_NAME; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUExternalTexture::getBrand);
    installGetterSetter(runtime, prototype, "label",
                        &GPUExternalTexture::getLabel,
                        &GPUExternalTexture::setLabel);
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
