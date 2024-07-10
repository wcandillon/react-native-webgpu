#pragma once

#include <future>
#include <memory>
#include <string>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBuffer : public m::HybridObject {
public:
  explicit GPUBuffer(wgpu::Buffer instance, std::string label)
      : HybridObject("GPUBuffer"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<MutableJSIBuffer> getMappedRange(std::optional<double> offset,
                                                   std::optional<double> size);
  void unmap();

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBuffer::getBrand, this);
    registerHybridMethod("getMappedRange", &GPUBuffer::getMappedRange, this);
    registerHybridMethod("unmap", &GPUBuffer::unmap, this);

    registerHybridGetter("label", &GPUBuffer::getLabel, this);
  }

private:
  wgpu::Buffer _instance;
  std::string _label;
};
} // namespace rnwgpu