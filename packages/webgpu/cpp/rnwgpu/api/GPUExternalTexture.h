#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunnerLegacy.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUExternalTexture : public m::HybridObject {
public:
  explicit GPUExternalTexture(wgpu::ExternalTexture instance, std::string label)
      : HybridObject("GPUExternalTexture"), _instance(instance), _label(label) {
  }

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUExternalTexture::getBrand, this);

    registerHybridGetter("label", &GPUExternalTexture::getLabel, this);
    registerHybridSetter("label", &GPUExternalTexture::setLabel, this);
  }

  inline const wgpu::ExternalTexture get() { return _instance; }

private:
  wgpu::ExternalTexture _instance;
  std::string _label;
};

} // namespace rnwgpu