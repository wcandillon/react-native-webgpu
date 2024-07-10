#pragma once

#include <future>
#include <memory>
#include <string>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUSampler : public m::HybridObject {
public:
  explicit GPUSampler(wgpu::Sampler instance, std::string label)
      : HybridObject("GPUSampler"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSampler::getBrand, this);

    registerHybridGetter("label", &GPUSampler::getLabel, this);
  }

private:
  wgpu::Sampler _instance;
  std::string _label;
};
} // namespace rnwgpu