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

  std::shared_ptr<MutableJSIBuffer> getMappedRange(std::optional<double> offset,
                                                   std::optional<double> size) {
    auto offsetParam = offset.value_or(0);
    auto sizeParam = size.value_or(static_cast<double>(_instance->GetSize()));
    auto o = static_cast<size_t>(offsetParam);
    auto s = static_cast<size_t>(sizeParam);
    Logger::logToConsole("getMappedRange: offset: %d, size: %d", offsetParam, sizeParam);
    auto usage = _instance->GetUsage();
    void *data = (usage & wgpu::BufferUsage::MapWrite)
                     ? _instance->GetMappedRange(o, s)
                     : const_cast<void *>(
                         _instance->GetConstMappedRange(o, s));
    return std::make_shared<MutableJSIBuffer>(data, s);
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
