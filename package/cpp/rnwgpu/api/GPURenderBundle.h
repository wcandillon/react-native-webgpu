#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundle : public m::HybridObject {
public:
  explicit GPURenderBundle(std::shared_ptr<wgpu::RenderBundle> instance)
      : HybridObject("GPURenderBundle"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<std::string> getLabel() {}

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundle::getBrand, this);

    registerHybridGetter("label", &GPURenderBundle::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::RenderBundle> _instance;
};
} // namespace rnwgpu