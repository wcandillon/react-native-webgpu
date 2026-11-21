#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "JSIConverter.h"
#include "VideoFrame.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// Descriptor for GPUDevice.importSharedTextureMemory: the buffer to import,
// either
//   - `source`, a NativeBuffer: the memory keeps it alive, and the textures
//     created from it carry its color space for importExternalTexture(), or
//   - `handle`, the raw native handle as a BigInt (an IOSurfaceRef on Apple
//     platforms, an AHardwareBuffer* on Android), which the caller keeps
//     alive for as long as the memory is in use.
struct GPUSharedTextureMemoryDescriptor {
  std::shared_ptr<VideoFrame> source;
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
      if (value.hasProperty(runtime, "source")) {
        auto prop = value.getProperty(runtime, "source");
        if (!prop.isUndefined() && !prop.isNull()) {
          result->source =
              JSIConverter<std::shared_ptr<rnwgpu::VideoFrame>>::fromJSI(
                  runtime, prop, false);
        }
      }
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
