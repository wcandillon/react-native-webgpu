#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

#include "AsyncRunner.h"
#include "GPUDevice.h"
#include "GPUDeviceDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapter : public m::HybridObject {
friend class GPU;
public:
  explicit GPUAdapter(wgpu::Adapter instance, std::shared_ptr<AsyncRunner> async)
      : HybridObject("GPUAdapter"), _instance(instance), _async(async) {}

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUDevice>>
  requestDevice(std::shared_ptr<GPUDeviceDescriptor> options);

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapter::getBrand, this);
    registerHybridMethod("requestDevice", &GPUAdapter::requestDevice, this);
  }

private:
  wgpu::Adapter _instance;
  std::shared_ptr<AsyncRunner> _async;
};
} // namespace rnwgpu
