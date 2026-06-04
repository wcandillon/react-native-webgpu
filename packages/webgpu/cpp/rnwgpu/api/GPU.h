#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

#include "Unions.h"

#include "NativeObject.h"

#include "rnwgpu/async/AsyncRunner.h"
#include "rnwgpu/async/AsyncTaskHandle.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

#include <webgpu/webgpu.h>

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPU : public NativeObject<GPU> {
public:
  static constexpr const char *CLASS_NAME = "GPU";

  // Creates and owns a default Dawn instance.
  explicit GPU(jsi::Runtime &runtime);
  // Uses an externally-owned Dawn instance (e.g. one provided by Skia
  // Graphite), instead of creating its own. The caller keeps ownership of the
  // instance and is responsible for keeping it alive for the lifetime of this
  // GPU object.
  GPU(jsi::Runtime &runtime, wgpu::Instance instance);

public:
  std::string getBrand() { return CLASS_NAME; }

  async::AsyncTaskHandle requestAdapter(
      std::optional<std::shared_ptr<GPURequestAdapterOptions>> options);
  wgpu::TextureFormat getPreferredCanvasFormat();

  std::unordered_set<std::string> getWgslLanguageFeatures();

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPU::getBrand);
    installMethod(runtime, prototype, "requestAdapter", &GPU::requestAdapter);
    installMethod(runtime, prototype, "getPreferredCanvasFormat",
                  &GPU::getPreferredCanvasFormat);
    installGetter(runtime, prototype, "wgslLanguageFeatures",
                  &GPU::getWgslLanguageFeatures);
  }

  inline const wgpu::Instance get() { return _instance; }
  inline std::shared_ptr<async::AsyncRunner> getAsyncRunner() { return _async; }

private:
  wgpu::Instance _instance;
  std::shared_ptr<async::AsyncRunner> _async;
};

} // namespace rnwgpu
