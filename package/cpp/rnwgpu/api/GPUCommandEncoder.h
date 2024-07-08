#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandEncoder : public m::HybridObject {
public:
  explicit GPUCommandEncoder(std::shared_ptr<wgpu::CommandEncoder> instance)
      : HybridObject("GPUCommandEncoder"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandEncoder::getBrand, this);

    registerHybridGetter("label", &GPUCommandEncoder::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::CommandEncoder> _instance;
};
} // namespace rnwgpu