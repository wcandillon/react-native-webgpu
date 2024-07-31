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

  wgpu::QueryType getType();
  uint32_t getCount();

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQuerySet::getBrand, this);
    registerHybridMethod("destroy", &GPUQuerySet::destroy, this);
    registerHybridGetter("type", &GPUQuerySet::getType, this);
    registerHybridGetter("count", &GPUQuerySet::getCount, this);
    registerHybridGetter("label", &GPUQuerySet::getLabel, this);
    registerHybridSetter("label", &GPUQuerySet::setLabel, this);
  }

  inline const wgpu::QuerySet get() { return _instance; }

private:
  wgpu::QuerySet _instance;
  std::string _label;
};

} // namespace rnwgpu