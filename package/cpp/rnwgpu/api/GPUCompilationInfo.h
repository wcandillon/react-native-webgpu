#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCompilationInfo : public m::HybridObject {
public:
  explicit GPUCompilationInfo(std::shared_ptr<wgpu::CompilationInfo> instance)
      : HybridObject("GPUCompilationInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCompilationInfo::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::CompilationInfo> _instance;
};
} // namespace rnwgpu