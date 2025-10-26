#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "rnwgpu/async/AsyncRunner.h"
#include "rnwgpu/async/AsyncTaskHandle.h"

#include "dawn/native/DawnNative.h"
#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

#include <webgpu/webgpu.h>

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  explicit GPU(jsi::Runtime& runtime);

public:
  std::string getBrand() { return _name; }

  async::AsyncTaskHandle requestAdapter(
      std::optional<std::shared_ptr<GPURequestAdapterOptions>> options);
  wgpu::TextureFormat getPreferredCanvasFormat();

  std::unordered_set<std::string> getWgslLanguageFeatures();

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
    registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
    registerHybridMethod("getPreferredCanvasFormat",
                         &GPU::getPreferredCanvasFormat, this);
    registerHybridGetter("wgslLanguageFeatures", &GPU::getWgslLanguageFeatures,
                         this);
  }

  inline const wgpu::Instance get() { return _instance; }

private:
  wgpu::Instance _instance;
  std::shared_ptr<async::AsyncRunner> _async;
};

} // namespace rnwgpu
