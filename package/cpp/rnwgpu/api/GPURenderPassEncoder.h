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

class GPURenderPassEncoder : public m::HybridObject {
public:
  explicit GPURenderPassEncoder(wgpu::RenderPassEncoder instance,
                                std::string label)
      : HybridObject("GPURenderPassEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPassEncoder::getBrand, this);

    registerHybridGetter("label", &GPURenderPassEncoder::getLabel, this);
  }

  // private:
  wgpu::RenderPassEncoder _instance;
  std::string _label;
};
} // namespace rnwgpu