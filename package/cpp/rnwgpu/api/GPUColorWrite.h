#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUColorWrite : public m::HybridObject {
public:
  GPUColorWrite() : HybridObject("GPUColorWrite") {}

public:
  wgpu::ColorWriteMask Red() { return wgpu::ColorWriteMask::Red; };
  wgpu::ColorWriteMask Green() { return wgpu::ColorWriteMask::Green; };
  wgpu::ColorWriteMask Blue() { return wgpu::ColorWriteMask::Blue; };
  wgpu::ColorWriteMask Alpha() { return wgpu::ColorWriteMask::Alpha; };
  wgpu::ColorWriteMask All() { return wgpu::ColorWriteMask::All; }

  void loadHybridMethods() override {
    registerHybridGetter("RED", &GPUColorWrite::Red, this);
    registerHybridGetter("GREEN", &GPUColorWrite::Green, this);
    registerHybridGetter("BLUE", &GPUColorWrite::Blue, this);
    registerHybridGetter("ALPHA", &GPUColorWrite::Alpha, this);
    registerHybridGetter("ALL", &GPUColorWrite::All, this);
  }
};
} // namespace rnwgpu