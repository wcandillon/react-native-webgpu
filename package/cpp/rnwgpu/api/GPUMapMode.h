#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUMapMode : public m::HybridObject {
public:
  GPUMapMode() : HybridObject("GPUMapMode") {}

public:
  wgpu::MapMode Read() { return wgpu::MapMode::Read; };
  wgpu::MapMode Write() { return wgpu::MapMode::Write; }

  void loadHybridMethods() override {
    registerHybridGetter("READ", &GPUMapMode::Read, this);
    registerHybridGetter("WRITE", &GPUMapMode::Write, this);
  }
};
} // namespace rnwgpu