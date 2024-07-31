#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBindGroup : public m::HybridObject {
public:
  explicit GPUBindGroup(wgpu::BindGroup instance, std::string label)
      : HybridObject("GPUBindGroup"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBindGroup::getBrand, this);

    registerHybridGetter("label", &GPUBindGroup::getLabel, this);
    registerHybridSetter("label", &GPUBindGroup::setLabel, this);
  }

  inline const wgpu::BindGroup get() { return _instance; }

private:
  wgpu::BindGroup _instance;
  std::string _label;
};

} // namespace rnwgpu