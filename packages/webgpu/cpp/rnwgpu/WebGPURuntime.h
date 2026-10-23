#pragma once

#include <mutex>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// Process-lifetime GPU state shared by every React instance, JS runtime and
// thread. It holds nothing tied to one React instance (no jsi::Runtime,
// CallInvoker or BlobModule): those stay with the per-instance module.
// Intentionally never destroyed, so there is no static-destruction ordering to
// get wrong and it outlives a React reload.
class WebGPURuntime {
public:
  static WebGPURuntime &shared();

  // The process-wide instance: Skia Graphite's when that is already loaded on
  // first use, otherwise our own. Safe to call from any thread.
  wgpu::Instance instance();

  // Creates a surface on the shared instance for a CAMetalLayer (Apple) or an
  // ANativeWindow (Android). Safe to call from any thread.
  wgpu::Surface makeSurface(void *nativeSurface);

  WebGPURuntime(const WebGPURuntime &) = delete;
  WebGPURuntime &operator=(const WebGPURuntime &) = delete;

private:
  WebGPURuntime() = default;

  std::once_flag _instanceOnce;
  wgpu::Instance _instance;
};

} // namespace rnwgpu
