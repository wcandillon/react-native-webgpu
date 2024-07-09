#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandBuffer : public m::HybridObject {
public:
  explicit GPUCommandBuffer(std::shared_ptr<wgpu::CommandBuffer> instance,
                            std::string label)
      : HybridObject("GPUCommandBuffer"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandBuffer::getBrand, this);

    registerHybridGetter("label", &GPUCommandBuffer::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::CommandBuffer> _instance;
  std::string _label;
};
} // namespace rnwgpu