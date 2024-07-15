#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"
#include "GPUCommandBuffer.h"
#include "GPUCommandBufferDescriptor.h"
#include "GPURenderPassDescriptor.h"
#include "GPURenderPassEncoder.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandEncoder : public m::HybridObject {
public:
  explicit GPUCommandEncoder(wgpu::CommandEncoder instance, std::string label)
      : HybridObject("GPUCommandEncoder"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPURenderPassEncoder>
  beginRenderPass(std::shared_ptr<GPURenderPassDescriptor> descriptor);
  void copyBufferToBuffer(std::shared_ptr<GPUBuffer> source,
                          uint64_t sourceOffset,
                          std::shared_ptr<GPUBuffer> destination,
                          uint64_t destinationOffset, uint64_t size);
  std::shared_ptr<GPUCommandBuffer>
  finish(std::shared_ptr<GPUCommandBufferDescriptor> descriptor);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandEncoder::getBrand, this);
    registerHybridMethod("beginRenderPass", &GPUCommandEncoder::beginRenderPass,
                         this);
    registerHybridMethod("copyBufferToBuffer",
                         &GPUCommandEncoder::copyBufferToBuffer, this);
    registerHybridMethod("finish", &GPUCommandEncoder::finish, this);

    registerHybridGetter("label", &GPUCommandEncoder::getLabel, this);
  }

  inline const wgpu::CommandEncoder get() { return _instance; }

private:
  wgpu::CommandEncoder _instance;
  std::string _label;
};
} // namespace rnwgpu