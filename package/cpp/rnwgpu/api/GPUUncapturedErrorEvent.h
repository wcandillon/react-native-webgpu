#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>
// #include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUUncapturedErrorEvent : public m::HybridObject {
public:
  explicit GPUUncapturedErrorEvent(
      std::shared_ptr<wgpu::UncapturedErrorEvent> instance)
      : HybridObject("GPUUncapturedErrorEvent"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUUncapturedErrorEvent::getBrand, this);
  }

private:
  std::shared_ptr<wgpu::UncapturedErrorEvent> _instance;
};
} // namespace rnwgpu