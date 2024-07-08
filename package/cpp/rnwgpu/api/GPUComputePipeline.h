#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePipeline : public m::HybridObject {
public:
  explicit GPUComputePipeline(std::shared_ptr<wgpu::ComputePipeline> instance,
                              std::string label)
      : HybridObject("GPUComputePipeline"), _instance(instance), _label(label) {
  }

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePipeline::getBrand, this);

    registerHybridGetter("label", &GPUComputePipeline::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::ComputePipeline> _instance;
  std::string _label;
};
} // namespace rnwgpu