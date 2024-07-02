#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  explicit GPUDevice(std::shared_ptr<wgpu::Device> instance)
      : HybridObject("GPUDevice"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
    _instance->CreateBuffer(descriptor->getInstance());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDevice::getBrand, this);
    registerHybridMethod("createBuffer", &GPUDevice::createBuffer, this);
  }

private:
  std::shared_ptr<wgpu::Device> _instance;
};
} // namespace rnwgpu