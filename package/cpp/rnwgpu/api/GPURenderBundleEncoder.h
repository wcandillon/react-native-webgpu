#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundleEncoder : public m::HybridObject {
public:
  explicit GPURenderBundleEncoder(
      std::shared_ptr<wgpu::RenderBundleEncoder> instance)
      : HybridObject("GPURenderBundleEncoder"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundleEncoder::getBrand, this);

    registerHybridGetter("label", &GPURenderBundleEncoder::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::RenderBundleEncoder> _instance;
};
} // namespace rnwgpu