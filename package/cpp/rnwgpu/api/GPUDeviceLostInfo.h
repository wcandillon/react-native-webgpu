#pragma once

#include <future>
#include <memory>
#include <string>

#include "MutableBuffer.h"
#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDeviceLostInfo : public m::HybridObject {
public:
  explicit GPUDeviceLostInfo(std::shared_ptr<wgpu::DeviceLostInfo> instance)
      : HybridObject("GPUDeviceLostInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDeviceLostInfo::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::DeviceLostInfo> _instance;
};
} // namespace rnwgpu