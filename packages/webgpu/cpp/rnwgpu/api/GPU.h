#pragma once

#include <future>
#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"
#include "dawn/dawn_proc.h"
#include "dawn/native/DawnNative.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU() : HybridObject("GPU") {
    DawnProcTable backendProcs = dawn::native::GetProcs();
    dawnProcSetProcs(&backendProcs);
    wgpu::InstanceDescriptor instanceDesc;
    instanceDesc.capabilities.timedWaitAnyEnable = true;
    instanceDesc.capabilities.timedWaitAnyMaxCount = 64;
    _instance = wgpu::CreateInstance(&instanceDesc);
    auto instance = &_instance;
    _async = std::make_shared<AsyncRunner>(instance);
  }

public:
  std::string getBrand() { return _name; }

  std::future<std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>>>
  requestAdapter(
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
  std::shared_ptr<AsyncRunner> _async;
};

} // namespace rnwgpu
