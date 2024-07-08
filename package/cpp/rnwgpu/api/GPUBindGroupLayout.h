#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBindGroupLayout : public m::HybridObject {
public:
  explicit GPUBindGroupLayout(std::shared_ptr<wgpu::BindGroupLayout> instance,
                              std::string label)
      : HybridObject("GPUBindGroupLayout"), _instance(instance), _label(label) {
  }

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBindGroupLayout::getBrand, this);

    registerHybridGetter("label", &GPUBindGroupLayout::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::BindGroupLayout> _instance;
  std::string _label;
};
} // namespace rnwgpu