#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCompilationMessage : public m::HybridObject {
public:
  explicit GPUCompilationMessage(wgpu::CompilationMessage instance)
      : HybridObject("GPUCompilationMessage"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCompilationMessage::getBrand, this);
  }

private:
  wgpu::CompilationMessage _instance;
};
} // namespace rnwgpu