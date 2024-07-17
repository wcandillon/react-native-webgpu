#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"
#include "Convertors.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCanvasContext : public m::HybridObject {
public:
  explicit GPUCanvasContext(wgpu::CanvasContext instance)
      : HybridObject("GPUCanvasContext"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCanvasContext::getBrand, this);
  }

  inline const wgpu::CanvasContext get() { return _instance; }

private:
  wgpu::CanvasContext _instance;
};

bool conv(wgpu::CanvasContext &out,
          const std::shared_ptr<GPUCanvasContext> &in) {
  out = in->get();
  return true;
}

} // namespace rnwgpu