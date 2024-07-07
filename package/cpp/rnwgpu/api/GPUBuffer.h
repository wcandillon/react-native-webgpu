#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBuffer : public m::HybridObject {
public:
  explicit GPUBuffer(std::shared_ptr<wgpu::Buffer> instance)
      : HybridObject("GPUBuffer"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<MutableJSIBuffer> getMappedRange(std::optional<double> offset,
                                                   std::optional<double> size) {
    auto aOffset = offset.value_or(0);
    auto aSize = size.value_or(WGPU_WHOLE_MAP_SIZE);
    auto result = _instance->GetMappedRange(aOffset, aSize);
    return std::make_shared<MutableJSIBuffer>(
        std::make_shared<MutableJSIBuffer>(result));
  }
  void unmap() { _instance->Unmap(); }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBuffer::getBrand, this);
    registerHybridMethod("getMappedRange", &GPUBuffer::getMappedRange, this);
    registerHybridMethod("unmap", &GPUBuffer::unmap, this);
  }

private:
  std::shared_ptr<wgpu::Buffer> _instance;
};
} // namespace rnwgpu