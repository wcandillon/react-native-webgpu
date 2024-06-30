#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapterInfo : public m::HybridObject {
public:
  explicit GPUAdapterInfo(std::shared_ptr<wgpu::AdapterInfo> instance)
      : HybridObject("GPUAdapterInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapterInfo::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::AdapterInfo> _instance;
};
} // namespace rnwgpu