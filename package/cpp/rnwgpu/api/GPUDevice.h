#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  GPUDevice(std::shared_ptr<wgpu::Device> instance)
      : HybridObject("GPUDevice"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDevice::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::Device> _instance;
};
} // namespace rnwgpu