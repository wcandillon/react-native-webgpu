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
  explicit GPURenderPipeline(std::shared_ptr<wgpu::RenderPipeline> instance,
                             std::string label)
      : HybridObject("GPURenderPipeline"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPipeline::getBrand, this);

    registerHybridGetter("label", &GPURenderPipeline::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::RenderPipeline> _instance;
  std::string _label;
};
} // namespace rnwgpu