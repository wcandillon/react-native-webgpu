#pragma once

#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  explicit GPUDevice(std::shared_ptr<wgpu::Device> instance)
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