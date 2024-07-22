#pragma once

#include <future>
#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUDevice.h"
#include "GPUDeviceDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapter : public m::HybridObject {
public:
  explicit GPUAdapter(wgpu::Adapter instance,
                      std::shared_ptr<AsyncRunner> async)
      : HybridObject("GPUAdapter"), _instance(instance), _async(async) {}

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUDevice>>
  requestDevice(std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor);

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapter::getBrand, this);
    registerHybridMethod("requestDevice", &GPUAdapter::requestDevice, this);
  }

  inline const wgpu::Adapter get() { return _instance; }

private:
  wgpu::Adapter _instance;
  std::shared_ptr<AsyncRunner> _async;
};

} // namespace rnwgpu