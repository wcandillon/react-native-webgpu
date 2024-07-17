#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"
#include "Convertors.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDeviceLostInfo : public m::HybridObject {
public:
  explicit GPUDeviceLostInfo(wgpu::DeviceLostInfo instance)
      : HybridObject("GPUDeviceLostInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDeviceLostInfo::getBrand, this);
  }

  inline const wgpu::DeviceLostInfo get() { return _instance; }

private:
  wgpu::DeviceLostInfo _instance;
};
} // namespace rnwgpu