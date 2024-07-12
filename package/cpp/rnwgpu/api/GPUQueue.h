#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "AsyncRunner.h"
#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUQueue : public m::HybridObject {
public:
  explicit GPUQueue(wgpu::Queue instance, std::shared_ptr<AsyncRunner> async,
                    std::string label)
      : HybridObject("GPUQueue"), _instance(instance), _async(async),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQueue::getBrand, this);

    registerHybridGetter("label", &GPUQueue::getLabel, this);
  }

private:
  wgpu::Queue _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
};
} // namespace rnwgpu