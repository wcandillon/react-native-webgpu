#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUSupportedLimits : public m::HybridObject {
public:
  explicit GPUSupportedLimits(wgpu::SupportedLimits instance)
      : HybridObject("GPUSupportedLimits"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSupportedLimits::getBrand, this);
  }

  inline const wgpu::SupportedLimits get() { return _instance; }

private:
  wgpu::SupportedLimits _instance;
};
} // namespace rnwgpu