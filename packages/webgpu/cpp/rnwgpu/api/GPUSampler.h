#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"


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
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSampler::getBrand, this);

    registerHybridGetter("label", &GPUSampler::getLabel, this);
    registerHybridSetter("label", &GPUSampler::setLabel, this);
  }

  inline const wgpu::Sampler get() { return _instance; }

private:
  wgpu::Sampler _instance;
  std::string _label;
};

} // namespace rnwgpu
