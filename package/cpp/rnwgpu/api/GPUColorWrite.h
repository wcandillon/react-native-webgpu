#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUColorWrite : public m::HybridObject {
public:
  GPUColorWrite() : HybridObject("GPUColorWrite") {}

public:
  double Red() { return static_cast<double>(wgpu::ColorWriteMask::Red); };
  double Green() { return static_cast<double>(wgpu::ColorWriteMask::Green); };
  double Blue() { return static_cast<double>(wgpu::ColorWriteMask::Blue); };
  double Alpha() { return static_cast<double>(wgpu::ColorWriteMask::Alpha); };
  double All() { return static_cast<double>(wgpu::ColorWriteMask::All); }

  void loadHybridMethods() override {
    registerHybridGetter("RED", &GPUColorWrite::Red, this);
    registerHybridGetter("GREEN", &GPUColorWrite::Green, this);
    registerHybridGetter("BLUE", &GPUColorWrite::Blue, this);
    registerHybridGetter("ALPHA", &GPUColorWrite::Alpha, this);
    registerHybridGetter("ALL", &GPUColorWrite::All, this);
  }
};
} // namespace rnwgpu