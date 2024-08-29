#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundle : public m::HybridObject {
public:
  explicit GPURenderBundle(wgpu::RenderBundle instance, std::string label)
      : HybridObject("GPURenderBundle"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundle::getBrand, this);

    registerHybridGetter("label", &GPURenderBundle::getLabel, this);
    registerHybridSetter("label", &GPURenderBundle::setLabel, this);
  }

  inline const wgpu::RenderBundle get() { return _instance; }

private:
  wgpu::RenderBundle _instance;
  std::string _label;
};

} // namespace rnwgpu