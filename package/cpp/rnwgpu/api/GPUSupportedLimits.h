#pragma once

#include <future>
#include <memory>
#include <string>

#include "MutableBuffer.h"
#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUSupportedLimits : public m::HybridObject {
public:
  explicit GPUSupportedLimits(std::shared_ptr<wgpu::SupportedLimits> instance)
      : HybridObject("GPUSupportedLimits"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSupportedLimits::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::SupportedLimits> _instance;
};
} // namespace rnwgpu