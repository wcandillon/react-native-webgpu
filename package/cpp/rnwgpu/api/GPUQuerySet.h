#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

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

  void destroy();

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQuerySet::getBrand, this);
    registerHybridMethod("destroy", &GPUQuerySet::destroy, this);

    registerHybridGetter("label", &GPUQuerySet::getLabel, this);
  }

  inline const wgpu::QuerySet get() { return _instance; }

private:
  wgpu::QuerySet _instance;
  std::string _label;
};

} // namespace rnwgpu