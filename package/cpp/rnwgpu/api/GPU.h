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
  explicit GPU(std::shared_ptr<wgpu::Instance> instance)
      : HybridObject("GPU"), _instance(instance) {}

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
  std::shared_ptr<wgpu::Instance> _instance;
};
} // namespace rnwgpu
