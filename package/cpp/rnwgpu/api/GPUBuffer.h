#pragma once

#include <future>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "ArrayBuffer.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBuffer : public m::HybridObject {
public:
  explicit GPUBuffer(wgpu::Buffer instance, std::shared_ptr<AsyncRunner> async,
                     std::string label)
      : HybridObject("GPUBuffer"), _instance(instance), _async(async),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::future<void> mapAsync(uint64_t modeIn,
                             std::optional<uint64_t> offset,
                             std::optional<uint64_t> size);
  std::shared_ptr<ArrayBuffer> getMappedRange(std::optional<size_t> offset,
                                              std::optional<size_t> size);
  void unmap();
  void destroy();

  uint64_t getSize();
  double getUsage();
  wgpu::BufferMapState getMapState();

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBuffer::getBrand, this);
    registerHybridMethod("mapAsync", &GPUBuffer::mapAsync, this);
    registerHybridMethod("getMappedRange", &GPUBuffer::getMappedRange, this);
    registerHybridMethod("unmap", &GPUBuffer::unmap, this);
    registerHybridMethod("destroy", &GPUBuffer::destroy, this);
    registerHybridGetter("size", &GPUBuffer::getSize, this);
    registerHybridGetter("usage", &GPUBuffer::getUsage, this);
    registerHybridGetter("mapState", &GPUBuffer::getMapState, this);
    registerHybridGetter("label", &GPUBuffer::getLabel, this);
  }

  inline const wgpu::Buffer get() { return _instance; }

private:
  wgpu::Buffer _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
  struct Mapping {
    uint64_t start;
    uint64_t end;
    inline bool Intersects(uint64_t s, uint64_t e) const {
      return s < end && e > start;
    }
    std::shared_ptr<ArrayBuffer> buffer;
  };
  std::vector<Mapping> mappings;
};

} // namespace rnwgpu
