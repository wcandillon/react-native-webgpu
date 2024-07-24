#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDeviceLostInfo : public m::HybridObject {
public:
  explicit GPUDeviceLostInfo(wgpu::DeviceLostReason instance)
      : HybridObject("GPUDeviceLostInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDeviceLostInfo::getBrand, this);
  }

  inline const wgpu::DeviceLostReason get() { return _instance; }

private:
  wgpu::DeviceLostReason _instance;
};

} // namespace rnwgpu
