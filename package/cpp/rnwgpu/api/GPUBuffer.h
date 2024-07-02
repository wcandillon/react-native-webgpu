#pragma once

#include <future>
#include <memory>
#include <string>
#include <android/log.h>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

#include "MutableBuffer.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBuffer : public m::HybridObject {
public:
  explicit GPUBuffer(std::shared_ptr<wgpu::Buffer> instance)
      : HybridObject("GPUBuffer"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<MutableJSIBuffer> getMappedRange(double offset,
                                                   double size) {
    auto offsetParam = static_cast<size_t>(offset);
    auto sizeParam = static_cast<size_t>(size);
    auto usage = _instance->GetUsage();
    void *data = (usage & wgpu::BufferUsage::MapWrite)
                     ? _instance->GetMappedRange(offsetParam )
                     : const_cast<void *>(
                         _instance->GetConstMappedRange(offsetParam));

    return std::make_shared<MutableJSIBuffer>(data, size);
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
