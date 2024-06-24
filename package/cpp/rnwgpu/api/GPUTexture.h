#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTexture : public m::HybridObject {
public:
  explicit GPUTexture(std::shared_ptr<wgpu::Texture> instance)
      : HybridObject("GPUTexture"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUTexture::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::Texture> _instance;
};
} // namespace rnwgpu