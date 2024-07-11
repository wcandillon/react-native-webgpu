#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

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

  std::future<void> mapAsync(size_t mode, std::optional<size_t> offset,
                             std::optional<size_t> size);
  std::shared_ptr<MutableJSIBuffer> getMappedRange(std::optional<size_t> offset,
                                                   std::optional<size_t> size);
  void unmap();

  size_t getSize();
  double getUsage();
  wgpu::BufferMapState getMapState();

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBuffer::getBrand, this);
    registerHybridMethod("mapAsync", &GPUBuffer::mapAsync, this);
    registerHybridMethod("getMappedRange", &GPUBuffer::getMappedRange, this);
    registerHybridMethod("unmap", &GPUBuffer::unmap, this);
    registerHybridGetter("size", &GPUBuffer::getSize, this);
    registerHybridGetter("usage", &GPUBuffer::getUsage, this);
    registerHybridGetter("mapState", &GPUBuffer::getMapState, this);
    registerHybridGetter("label", &GPUBuffer::getLabel, this);
  }

private:
  wgpu::Buffer _instance;
  std::string _label;
  struct Mapping {
    uint64_t start;
    uint64_t end;
    inline bool Intersects(uint64_t s, uint64_t e) const {
      return s < end && e > start;
    }
    std::shared_ptr<MutableJSIBuffer> buffer;
  };
  std::vector<Mapping> mappings;
};
} // namespace rnwgpu
