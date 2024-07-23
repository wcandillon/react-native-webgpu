#pragma once

#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandBuffer : public m::HybridObject {
public:
  explicit GPUCommandBuffer(wgpu::CommandBuffer instance, std::string label)
      : HybridObject("GPUCommandBuffer"), _instance(std::move(instance)),
        _label(std::move(label)) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandBuffer::getBrand, this);

    registerHybridGetter("label", &GPUCommandBuffer::getLabel, this);
  }

  inline const wgpu::CommandBuffer get() { return _instance; }

private:
  wgpu::CommandBuffer _instance;
  std::string _label;
};

} // namespace rnwgpu