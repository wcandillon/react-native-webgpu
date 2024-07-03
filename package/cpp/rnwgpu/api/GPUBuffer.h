#pragma once

#include <future>
#include <memory>
#include <string>

#include "MutableBuffer.h"

#include <RNFHybridObject.h>
#include "Logger.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBuffer : public m::HybridObject {
public:
  explicit GPUBuffer(std::shared_ptr<wgpu::Buffer> instance)
      : HybridObject("GPUBuffer"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<MutableJSIBuffer> getMappedRange(std::optional<size_t> offset,
                                                   std::optional<size_t> size) {
    auto offsetParam = offset.value_or(0);
    auto sizeParam = size.value_or(static_cast<size_t>(_instance->GetSize()));
    Logger::logToConsole("getMappedRange: offset: %d, size: %d", offsetParam, sizeParam);
    auto usage = _instance->GetUsage();
    void *data = (usage & wgpu::BufferUsage::MapWrite)
                     ? _instance->GetMappedRange(0)
                     : const_cast<void *>(
                         _instance->GetConstMappedRange(0));
    return std::make_shared<MutableJSIBuffer>(data, sizeParam);
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
