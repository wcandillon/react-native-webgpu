#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUSharedFence.h"
#include "GPUSharedFenceState.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "NativeFrameImport.h"
#include "VideoFrame.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUSharedTextureMemory : public NativeObject<GPUSharedTextureMemory> {
public:
  static constexpr const char *CLASS_NAME = "GPUSharedTextureMemory";

  // `source` is the NativeBuffer the memory was imported from, null for an
  // import from a raw handle: the memory keeps it alive, and its color space
  // goes to the textures created from the memory (see SharedTextureOrigin).
  explicit GPUSharedTextureMemory(wgpu::Device device,
                                  wgpu::SharedTextureMemory instance,
                                  std::string label,
                                  std::shared_ptr<VideoFrame> source = nullptr)
      : NativeObject(CLASS_NAME), _device(std::move(device)),
        _instance(std::move(instance)), _source(std::move(source)),
        _label(std::move(label)) {
    VideoFrameColorSpace colorSpace;
    if (_source) {
      colorSpace = _source->handle().colorSpace;
    }
    _origin = std::make_shared<native_frame::SharedTextureOrigin>(
        native_frame::SharedTextureOrigin{_instance, colorSpace, _label});
  }

public:
  std::string getBrand() { return CLASS_NAME; }

  std::shared_ptr<GPUTexture> createTexture(
      std::optional<std::shared_ptr<GPUTextureDescriptor>> descriptor);

  // Optional `fences` are wait fences: Dawn waits for each to reach its
  // signaledValue before writing the surface. Throws on failure.
  void beginAccess(
      std::shared_ptr<GPUTexture> texture, bool initialized,
      std::optional<std::vector<std::shared_ptr<GPUSharedFenceState>>> fences);

  // endAccess(texture) -> { initialized, fences: { fence, signaledValue }[] }
  // Surfaces the fences Dawn produced for the access. Throws on failure.
  jsi::Value endAccess(jsi::Runtime &runtime, const jsi::Value &thisVal,
                       const jsi::Value *args, size_t count);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _origin->label = label;
    if (_instance) {
      _instance.SetLabel(_label.c_str());
    }
  }

  // Releases the memory now (the imported native buffer and Dawn's import of
  // it) rather than when the JS object is garbage collected. End the access
  // first (destroying during one is a validation error), and destroy the
  // textures created from it: they keep what they use until then.
  void destroy();

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand",
                  &GPUSharedTextureMemory::getBrand);
    installMethod(runtime, prototype, "createTexture",
                  &GPUSharedTextureMemory::createTexture);
    installMethod(runtime, prototype, "beginAccess",
                  &GPUSharedTextureMemory::beginAccess);
    installMethod(runtime, prototype, "endAccess",
                  &GPUSharedTextureMemory::endAccess);
    installMethod(runtime, prototype, "destroy",
                  &GPUSharedTextureMemory::destroy);
    installGetterSetter(runtime, prototype, "label",
                        &GPUSharedTextureMemory::getLabel,
                        &GPUSharedTextureMemory::setLabel);
  }

  inline const wgpu::SharedTextureMemory get() { return _instance; }

private:
  // Reports a use of the destroyed memory the way Dawn reports an invalid
  // object: a validation error, caught by error scopes or else delivered to
  // the device's uncapturederror event.
  void injectDestroyedError(const char *method);

  wgpu::Device _device;
  wgpu::SharedTextureMemory _instance;
  std::shared_ptr<VideoFrame> _source;
  // Shared with every texture created from the memory; destroy() clears its
  // reference to the import.
  std::shared_ptr<native_frame::SharedTextureOrigin> _origin;
  // Kept by destroy(): like Dawn, createTexture() describes the error texture
  // it then returns with them.
  wgpu::SharedTextureMemoryProperties _properties;
  // The textures with an access begun and not yet ended, so destroy() can
  // report an access left open. Dawn doesn't track that across a release.
  std::vector<wgpu::Texture> _accessed;
  std::string _label;
};

} // namespace rnwgpu
