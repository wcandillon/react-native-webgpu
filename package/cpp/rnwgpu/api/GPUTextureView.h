#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTextureView : public m::HybridObject {
public:
  explicit GPUTextureView(std::shared_ptr<wgpu::TextureView> instance)
      : HybridObject("GPUTextureView"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUTextureView::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::TextureView> _instance;
};
} // namespace rnwgpu