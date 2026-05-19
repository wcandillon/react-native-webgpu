#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "JSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// Descriptor for GPUDevice.importSharedTextureMemory.
//
// `handle` is the raw native handle as a uintptr_t (passed as BigInt from JS):
//   - Apple platforms: IOSurfaceRef
//   - Android: AHardwareBuffer*
//
// Lifetime: the caller is responsible for keeping the underlying object alive
// for as long as this shared memory is in use. The VideoFrame helper handles
// this automatically when the handle came from PlatformContext.loadVideoFrame.
struct GPUSharedTextureMemoryDescriptor {
  void *handle = nullptr;
  std::optional<std::string> label;
};

} // namespace rnwgpu

namespace rnwgpu {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUSharedTextureMemoryDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUSharedTextureMemoryDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result =
        std::make_shared<rnwgpu::GPUSharedTextureMemoryDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "handle")) {
        auto prop = value.getProperty(runtime, "handle");
        result->handle =
            JSIConverter<void *>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter<std::optional<std::string>>::fromJSI(
            runtime, prop, false);
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime & /*runtime*/,
        std::shared_ptr<rnwgpu::GPUSharedTextureMemoryDescriptor> /*arg*/) {
    throw std::runtime_error(
        "Invalid GPUSharedTextureMemoryDescriptor::toJSI()");
  }
};

} // namespace rnwgpu
