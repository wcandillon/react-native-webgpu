#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUSharedFence.h"
#include "GPUSharedFenceState.h"
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
    if (_instance) {
      _instance.SetLabel(_label.c_str());
    }
  }

  // Non-spec RN extension: releases the memory now (the imported native
  // buffer and Dawn's import of it) rather than when the JS object is
  // garbage collected. The textures created from it should be destroyed
  // first: they keep what they use until then.
  void destroy() { _instance = nullptr; }

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
  wgpu::SharedTextureMemory _instance;
  std::string _label;
};

} // namespace rnwgpu
