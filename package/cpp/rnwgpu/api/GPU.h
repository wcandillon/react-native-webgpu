#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  explicit GPU(std::shared_ptr<wgpu::Instance> instance)
      : HybridObject("GPU"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUAdapter>>
  requestAdapter(std::shared_ptr<wgpu::RequestAdapterOptions> options);

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
    registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
  }

private:
  std::shared_ptr<wgpu::Instance> _instance;
};
} // namespace rnwgpu