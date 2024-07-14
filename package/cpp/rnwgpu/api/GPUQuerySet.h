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

class GPUQuerySet : public m::HybridObject {
public:
  explicit GPUQuerySet(wgpu::QuerySet instance, std::string label)
      : HybridObject("GPUQuerySet"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQuerySet::getBrand, this);

    registerHybridGetter("label", &GPUQuerySet::getLabel, this);
  }

  inline const wgpu::QuerySet get() { return _instance; }

private:
  wgpu::QuerySet _instance;
  std::string _label;
};
} // namespace rnwgpu