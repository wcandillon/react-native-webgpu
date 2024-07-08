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
      std::shared_ptr<wgpu::RenderBundleEncoder> instance, std::string label)
      : HybridObject("GPURenderBundleEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundleEncoder::getBrand, this);

    registerHybridGetter("label", &GPURenderBundleEncoder::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::RenderBundleEncoder> _instance;
  std::string _label;
};
} // namespace rnwgpu