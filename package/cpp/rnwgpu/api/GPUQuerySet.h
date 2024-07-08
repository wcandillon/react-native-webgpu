#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUQuerySet : public m::HybridObject {
public:
  explicit GPUQuerySet(std::shared_ptr<wgpu::QuerySet> instance)
      : HybridObject("GPUQuerySet"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQuerySet::getBrand, this);

    registerHybridGetter("label", &GPUQuerySet::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::QuerySet> _instance;
};
} // namespace rnwgpu