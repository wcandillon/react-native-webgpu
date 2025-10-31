#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "rnwgpu/async/AsyncRunner.h"
#include "rnwgpu/async/AsyncTaskHandle.h"

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
                      std::shared_ptr<async::AsyncRunner> async)
      : HybridObject("GPUAdapter"), _instance(instance), _async(async) {}

public:
  std::string getBrand() { return _name; }

  async::AsyncTaskHandle
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

  size_t getMemoryPressure() override { return 1024; }

private:
  wgpu::Adapter _instance;
  std::shared_ptr<async::AsyncRunner> _async;
};

} // namespace rnwgpu
