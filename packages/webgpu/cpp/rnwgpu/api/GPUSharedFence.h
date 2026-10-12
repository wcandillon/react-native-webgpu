#pragma once

#include <memory>
#include <string>

#include "NativeObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// Wraps a wgpu::SharedFence: a native GPU sync primitive (id<MTLSharedEvent> on
// Apple, sync-fd / VkSemaphore on Android).
class GPUSharedFence : public NativeObject<GPUSharedFence> {
public:
  static constexpr const char *CLASS_NAME = "GPUSharedFence";

  explicit GPUSharedFence(wgpu::SharedFence instance, std::string label)
      : NativeObject(CLASS_NAME), _instance(std::move(instance)),
        _label(std::move(label)) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  // export() -> { type, handle }: exposes the native handle (as a BigInt) so
  // app code can wait on or signal the fence. The caller owns the returned
  // handle (e.g. an exported sync-fd must be close()d).
  jsi::Value exportInfo(jsi::Runtime &runtime, const jsi::Value &thisVal,
                        const jsi::Value *args, size_t count);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    if (_instance) {
      _instance.SetLabel(_label.c_str());
    }
  }

  // Non-spec RN extension: releases the fence now (closing its sync fd on
  // Android) rather than when the JS object is garbage collected, which a
  // runtime making fences every frame may not do for a while. Dawn keeps its
  // own reference as long as it needs the fence (a beginAccess wait).
  void destroy() { _instance = nullptr; }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUSharedFence::getBrand);
    installMethod(runtime, prototype, "export", &GPUSharedFence::exportInfo);
    installMethod(runtime, prototype, "destroy", &GPUSharedFence::destroy);
    installGetterSetter(runtime, prototype, "label", &GPUSharedFence::getLabel,
                        &GPUSharedFence::setLabel);
  }

  inline wgpu::SharedFence get() { return _instance; }

private:
  wgpu::SharedFence _instance;
  std::string _label;
};

} // namespace rnwgpu
