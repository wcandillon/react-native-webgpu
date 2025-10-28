#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBindGroupLayout : public m::HybridObject {
public:
  explicit GPUBindGroupLayout(wgpu::BindGroupLayout instance, std::string label)
      : HybridObject("GPUBindGroupLayout"), _instance(instance), _label(label) {
  }

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBindGroupLayout::getBrand, this);

    registerHybridGetter("label", &GPUBindGroupLayout::getLabel, this);
    registerHybridSetter("label", &GPUBindGroupLayout::setLabel, this);
  }

  inline const wgpu::BindGroupLayout get() { return _instance; }

  size_t getMemoryPressure() override {
    // Bind group layouts define the structure/schema for bind groups
    // They store binding descriptors, types, and validation info
    // Estimate: 512 bytes per layout (smaller than actual bind groups)
    return 512;
  }

private:
  wgpu::BindGroupLayout _instance;
  std::string _label;
};

} // namespace rnwgpu
