#pragma once

#include <future>
#include <memory>
#include <string>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandEncoder : public m::HybridObject {
public:
  explicit GPUCommandEncoder(wgpu::CommandEncoder instance, std::string label)
      : HybridObject("GPUCommandEncoder"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandEncoder::getBrand, this);

    registerHybridGetter("label", &GPUCommandEncoder::getLabel, this);
  }

private:
  wgpu::CommandEncoder _instance;
  std::string _label;
};
} // namespace rnwgpu