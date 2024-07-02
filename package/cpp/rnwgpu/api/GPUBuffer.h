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
    __android_log_print(ANDROID_LOG_DEBUG, "GPUBuffer", "offset: %f", offset);
    __android_log_print(ANDROID_LOG_DEBUG, "GPUBuffer", "size: %f", size);
    auto result =
        _instance->GetMappedRange(0);
    __android_log_print(ANDROID_LOG_DEBUG, "GPUBuffer", "result address: %p", result);
    return std::make_shared<MutableJSIBuffer>(result, size);
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
