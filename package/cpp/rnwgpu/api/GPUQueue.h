#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUQueue : public m::HybridObject {
public:
  explicit GPUQueue(std::shared_ptr<wgpu::Queue> instance, std::string label)
      : HybridObject("GPUQueue"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQueue::getBrand, this);

    registerHybridGetter("label", &GPUQueue::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::Queue> _instance;
  std::string _label;
};
} // namespace rnwgpu