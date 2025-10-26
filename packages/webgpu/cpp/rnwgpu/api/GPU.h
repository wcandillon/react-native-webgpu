#pragma once

#include <future>
#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunnerLegacy.h"

#include "dawn/native/DawnNative.h"
#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

#include <webgpu/webgpu.h>

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU() : HybridObject("GPU") {
    static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor instanceDesc{.requiredFeatureCount = 1,
                                          .requiredFeatures = &kTimedWaitAny};

    // For limits:
    wgpu::InstanceLimits limits{.timedWaitAnyMaxCount = 64};
    instanceDesc.requiredLimits = &limits;
    _instance = wgpu::CreateInstance(&instanceDesc);
    auto instance = &_instance;
    _async = std::make_shared<AsyncRunnerLegacy>(instance);
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
  std::shared_ptr<AsyncRunnerLegacy> _async;
};

} // namespace rnwgpu
