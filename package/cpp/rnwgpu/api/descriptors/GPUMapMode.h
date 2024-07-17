#pragma once
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUMapMode : public m::HybridObject {
public:
  GPUMapMode() : HybridObject("GPUMapMode") {}

public:
  double Read() { return static_cast<double>(wgpu::MapMode::Read); }
  double Write() { return static_cast<double>(wgpu::MapMode::Write); }

  void loadHybridMethods() override {
    registerHybridGetter("READ", &GPUMapMode::Read, this);
    registerHybridGetter("WRITE", &GPUMapMode::Write, this);
  }
};
} // namespace rnwgpu