#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUSampler : public m::HybridObject {
public:
  explicit GPUSampler(std::shared_ptr<wgpu::Sampler> instance)
      : HybridObject("GPUSampler"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSampler::getBrand, this);

    registerHybridGetter("label", &GPUSampler::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::Sampler> _instance;
};
} // namespace rnwgpu