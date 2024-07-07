#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderPipeline : public m::HybridObject {
public:
  explicit GPURenderPipeline(std::shared_ptr<wgpu::RenderPipeline> instance)
      : HybridObject("GPURenderPipeline"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPipeline::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::RenderPipeline> _instance;
};
} // namespace rnwgpu