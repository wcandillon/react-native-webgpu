#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUPipelineLayout : public m::HybridObject {
public:
  explicit GPUPipelineLayout(std::shared_ptr<wgpu::PipelineLayout> instance,
                             std::string label)
      : HybridObject("GPUPipelineLayout"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUPipelineLayout::getBrand, this);

    registerHybridGetter("label", &GPUPipelineLayout::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::PipelineLayout> _instance;
  std::string _label;
};
} // namespace rnwgpu