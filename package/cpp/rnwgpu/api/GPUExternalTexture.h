#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUExternalTexture : public m::HybridObject {
public:
  explicit GPUExternalTexture(std::shared_ptr<wgpu::ExternalTexture> instance)
      : HybridObject("GPUExternalTexture"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUExternalTexture::getBrand, this);

    registerHybridGetter("label", &GPUExternalTexture::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::ExternalTexture> _instance;
};
} // namespace rnwgpu