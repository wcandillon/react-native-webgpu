#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTexture : public m::HybridObject {
public:
  explicit GPUTexture(std::shared_ptr<wgpu::Texture> instance)
      : HybridObject("GPUTexture"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUTexture::getBrand, this);

    registerHybridGetter("label", &GPUTexture::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::Texture> _instance;
};
} // namespace rnwgpu