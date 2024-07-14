#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandEncoder : public m::HybridObject {
public:
  explicit GPUCommandEncoder(wgpu::CommandEncoder instance, std::string label)
      : HybridObject("GPUCommandEncoder"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  void copyBufferToBuffer(std::shared_ptr<GPUBuffer> source,
                          uint64_t sourceOffset,
                          std::shared_ptr<GPUBuffer> destination,
                          uint64_t destinationOffset, uint64_t size);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandEncoder::getBrand, this);
    registerHybridMethod("copyBufferToBuffer",
                         &GPUCommandEncoder::copyBufferToBuffer, this);

    registerHybridGetter("label", &GPUCommandEncoder::getLabel, this);
  }

  // private:
  wgpu::CommandEncoder _instance;
  std::string _label;
};
} // namespace rnwgpu