#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "ArrayBuffer.h"
#include "GPUBuffer.h"
#include "GPUCommandBuffer.h"

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

  void submit(std::vector<std::shared_ptr<GPUCommandBuffer>> commandBuffers);
  std::future<void> onSubmittedWorkDone();
  void writeBuffer(std::shared_ptr<GPUBuffer> buffer, uint64_t bufferOffset,
                   std::shared_ptr<ArrayBuffer> data,
                   std::optional<uint64_t> dataOffsetElements,
                   std::optional<size_t> sizeElements);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUQueue::getBrand, this);
    registerHybridMethod("submit", &GPUQueue::submit, this);
    registerHybridMethod("onSubmittedWorkDone", &GPUQueue::onSubmittedWorkDone,
                         this);
    registerHybridMethod("writeBuffer", &GPUQueue::writeBuffer, this);

    registerHybridGetter("label", &GPUQueue::getLabel, this);
  }

  inline const wgpu::Queue get() { return _instance; }

private:
  wgpu::Queue _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
};

} // namespace rnwgpu