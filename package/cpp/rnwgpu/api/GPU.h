#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  explicit GPU(std::shared_ptr<wgpu::Instance> instance)
      : HybridObject("GPU"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUAdapter>> requestAdapter() {
    return std::async(std::launch::async,
                      [=]() { return std::make_shared<GPUAdapter>(); });
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
    registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
  }

private:
  std::shared_ptr<wgpu::Instance> _instance;
};
} // namespace rnwgpu