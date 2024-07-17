#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUUncapturedErrorEvent : public m::HybridObject {
public:
  explicit GPUUncapturedErrorEvent(wgpu::UncapturedErrorEvent instance)
      : HybridObject("GPUUncapturedErrorEvent"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUUncapturedErrorEvent::getBrand, this);
  }

  inline const wgpu::UncapturedErrorEvent get() { return _instance; }

private:
  wgpu::UncapturedErrorEvent _instance;
};

} // namespace rnwgpu