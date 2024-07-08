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
  explicit GPUQuerySet(std::shared_ptr<wgpu::QuerySet> instance,
                       std::string label)
      : HybridObject("GPUQuerySet"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQuerySet::getBrand, this);

    registerHybridGetter("label", &GPUQuerySet::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::QuerySet> _instance;
  std::string _label;
};
} // namespace rnwgpu