#pragma once

#include <future>
#include <memory>
#include <string>
#include <unordered_set>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunnerLegacy.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUAdapterInfo.h"
#include "GPUDevice.h"
#include "GPUDeviceDescriptor.h"
#include "GPUSupportedLimits.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapter : public m::HybridObject {
public:
  explicit GPUAdapter(wgpu::Adapter instance,
                      std::shared_ptr<AsyncRunnerLegacy> async)
      : HybridObject("GPUAdapter"), _instance(instance), _async(async) {}

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUDevice>>
  requestDevice(std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor);

  std::unordered_set<std::string> getFeatures();
  std::shared_ptr<GPUSupportedLimits> getLimits();
  std::shared_ptr<GPUAdapterInfo> getInfo();

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapter::getBrand, this);
    registerHybridMethod("requestDevice", &GPUAdapter::requestDevice, this);
    registerHybridGetter("features", &GPUAdapter::getFeatures, this);
    registerHybridGetter("limits", &GPUAdapter::getLimits, this);
    registerHybridGetter("info", &GPUAdapter::getInfo, this);
  }

  inline const wgpu::Adapter get() { return _instance; }

private:
  wgpu::Adapter _instance;
  std::shared_ptr<AsyncRunnerLegacy> _async;
};

} // namespace rnwgpu