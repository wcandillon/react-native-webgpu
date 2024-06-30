#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUSampler : public m::HybridObject {
public:
  explicit GPUSampler(std::shared_ptr<wgpu::Sampler> instance)
      : HybridObject("GPUSampler"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSampler::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::Sampler> _instance;
};
} // namespace rnwgpu