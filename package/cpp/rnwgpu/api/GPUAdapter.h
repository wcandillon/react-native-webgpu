#pragma once

#include <memory>
#include <string>
#include <future>
#include <vector>

#include "Unions.h"
#include "Convertors.h"
#include <RNFHybridObject.h>

#include "AsyncRunner.h"
#include "ArrayBuffer.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUDeviceDescriptor.h"
#include "GPUDevice.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapter : public m::HybridObject {
public:
    explicit GPUAdapter(wgpu::Adapter instance, std::shared_ptr<AsyncRunner> async) : HybridObject("GPUAdapter"), _instance(instance), _async(async) {}

public:
  std::string getBrand() { return _name; }


  std::future<std::shared_ptr<GPUDevice>> requestDevice(std::shared_ptr<GPUDeviceDescriptor> options);

  

  

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapter::getBrand, this);
    registerHybridMethod("requestDevice", &GPUAdapter::requestDevice, this);
    
    
  }
  
  inline const wgpu::Adapter get() {
    return _instance;
  }

 private:
  wgpu::Adapter _instance;
std::shared_ptr<AsyncRunner> _async;
  
};
} // namespace rnwgpu