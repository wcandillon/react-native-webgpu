#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePassEncoder : public m::HybridObject {
public:
  explicit GPUComputePassEncoder(wgpu::ComputePassEncoder instance,
                                 std::string label)
      : HybridObject("GPUComputePassEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePassEncoder::getBrand, this);

    registerHybridGetter("label", &GPUComputePassEncoder::getLabel, this);
  }

private:
  wgpu::ComputePassEncoder _instance;
  std::string _label;
};
} // namespace rnwgpu