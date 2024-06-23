#pragma once

#include <future>
#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"
#include <RNFHybridObject.h>

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU(): HybridObject("GPU") {
    wgpu::InstanceDescriptor instanceDesc;
    instanceDesc.features.timedWaitAnyEnable = true;
    instanceDesc.features.timedWaitAnyMaxCount = 64;
    _instance = wgpu::CreateInstance(&instanceDesc);
  }

public:
  std::future<std::shared_ptr<GPUAdapter>>
  requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
  wgpu::RequestAdapterOptions defaultOptions;
  // Create a shared_ptr to GPUAdapter
  return std::async(std::launch::async,
                    [=]() { return std::make_shared<GPUAdapter>(); });
}

  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
    registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
  }

  private:
    wgpu::Instance _instance;
  };
} // namespace rnwgpu
