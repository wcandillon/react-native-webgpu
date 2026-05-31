#pragma once

#include <memory>
#include <optional>
#include <string>

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUSharedTextureMemory : public NativeObject<GPUSharedTextureMemory> {
public:
  static constexpr const char *CLASS_NAME = "GPUSharedTextureMemory";

  explicit GPUSharedTextureMemory(wgpu::SharedTextureMemory instance,
                                  std::string label)
      : NativeObject(CLASS_NAME), _instance(std::move(instance)),
        _label(std::move(label)) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  std::shared_ptr<GPUTexture>
  createTexture(std::optional<std::shared_ptr<GPUTextureDescriptor>> descriptor);

  // Returns true on success. Marks the shared memory as initialized so the
  // texture's content is preserved (or not). Callers that want fence-based
  // synchronization should pass fences via beginAccess descriptor (not yet
  // exposed - we currently take the implicit/no-fence path that matches the
  // most common RN use cases: still images, single-producer video frames).
  bool beginAccess(std::shared_ptr<GPUTexture> texture, bool initialized);

  // Returns true on success. Drops any fences produced by end-access (we do
  // not yet surface them to JS).
  bool endAccess(std::shared_ptr<GPUTexture> texture);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand",
                  &GPUSharedTextureMemory::getBrand);
    installMethod(runtime, prototype, "createTexture",
                  &GPUSharedTextureMemory::createTexture);
    installMethod(runtime, prototype, "beginAccess",
                  &GPUSharedTextureMemory::beginAccess);
    installMethod(runtime, prototype, "endAccess",
                  &GPUSharedTextureMemory::endAccess);
    installGetterSetter(runtime, prototype, "label",
                        &GPUSharedTextureMemory::getLabel,
                        &GPUSharedTextureMemory::setLabel);
  }

  inline const wgpu::SharedTextureMemory get() { return _instance; }

private:
  wgpu::SharedTextureMemory _instance;
  std::string _label;
};

} // namespace rnwgpu
