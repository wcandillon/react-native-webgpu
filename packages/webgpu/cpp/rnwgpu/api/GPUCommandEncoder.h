#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunnerLegacy.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"
#include "GPUCommandBuffer.h"
#include "GPUCommandBufferDescriptor.h"
#include "GPUComputePassDescriptor.h"
#include "GPUComputePassEncoder.h"
#include "GPUExtent3D.h"
#include "GPUImageCopyBuffer.h"
#include "GPUImageCopyTexture.h"
#include "GPUQuerySet.h"
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
  std::shared_ptr<GPUComputePassEncoder> beginComputePass(
      std::optional<std::shared_ptr<GPUComputePassDescriptor>> descriptor);
  void copyBufferToBuffer(std::shared_ptr<GPUBuffer> source,
                          uint64_t sourceOffset,
                          std::shared_ptr<GPUBuffer> destination,
                          uint64_t destinationOffset, uint64_t size);
  void copyBufferToTexture(std::shared_ptr<GPUImageCopyBuffer> source,
                           std::shared_ptr<GPUImageCopyTexture> destination,
                           std::shared_ptr<GPUExtent3D> copySize);
  void copyTextureToBuffer(std::shared_ptr<GPUImageCopyTexture> source,
                           std::shared_ptr<GPUImageCopyBuffer> destination,
                           std::shared_ptr<GPUExtent3D> copySize);
  void copyTextureToTexture(std::shared_ptr<GPUImageCopyTexture> source,
                            std::shared_ptr<GPUImageCopyTexture> destination,
                            std::shared_ptr<GPUExtent3D> copySize);
  void clearBuffer(std::shared_ptr<GPUBuffer> buffer,
                   std::optional<uint64_t> offset,
                   std::optional<uint64_t> size);
  void resolveQuerySet(std::shared_ptr<GPUQuerySet> querySet,
                       uint32_t firstQuery, uint32_t queryCount,
                       std::shared_ptr<GPUBuffer> destination,
                       uint64_t destinationOffset);
  std::shared_ptr<GPUCommandBuffer>
  finish(std::optional<std::shared_ptr<GPUCommandBufferDescriptor>> descriptor);
  void pushDebugGroup(std::string groupLabel);
  void popDebugGroup();
  void insertDebugMarker(std::string markerLabel);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandEncoder::getBrand, this);
    registerHybridMethod("beginRenderPass", &GPUCommandEncoder::beginRenderPass,
                         this);
    registerHybridMethod("beginComputePass",
                         &GPUCommandEncoder::beginComputePass, this);
    registerHybridMethod("copyBufferToBuffer",
                         &GPUCommandEncoder::copyBufferToBuffer, this);
    registerHybridMethod("copyBufferToTexture",
                         &GPUCommandEncoder::copyBufferToTexture, this);
    registerHybridMethod("copyTextureToBuffer",
                         &GPUCommandEncoder::copyTextureToBuffer, this);
    registerHybridMethod("copyTextureToTexture",
                         &GPUCommandEncoder::copyTextureToTexture, this);
    registerHybridMethod("clearBuffer", &GPUCommandEncoder::clearBuffer, this);
    registerHybridMethod("resolveQuerySet", &GPUCommandEncoder::resolveQuerySet,
                         this);
    registerHybridMethod("finish", &GPUCommandEncoder::finish, this);
    registerHybridMethod("pushDebugGroup", &GPUCommandEncoder::pushDebugGroup,
                         this);
    registerHybridMethod("popDebugGroup", &GPUCommandEncoder::popDebugGroup,
                         this);
    registerHybridMethod("insertDebugMarker",
                         &GPUCommandEncoder::insertDebugMarker, this);

    registerHybridGetter("label", &GPUCommandEncoder::getLabel, this);
    registerHybridSetter("label", &GPUCommandEncoder::setLabel, this);
  }

  inline const wgpu::CommandEncoder get() { return _instance; }

private:
  wgpu::CommandEncoder _instance;
  std::string _label;
};

} // namespace rnwgpu