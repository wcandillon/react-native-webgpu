#pragma once

#include <future>
#include <memory>
#include <string>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  explicit GPUDevice(wgpu::Device instance, std::string label)
      : HybridObject("GPUDevice"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPUBuffer>
  createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDevice::getBrand, this);
    registerHybridMethod("createBuffer", &GPUDevice::createBuffer, this);

    registerHybridGetter("label", &GPUDevice::getLabel, this);
  }

private:
  wgpu::Device _instance;
  std::string _label;
};
} // namespace rnwgpu