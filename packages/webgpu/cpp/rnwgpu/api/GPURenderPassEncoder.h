#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
#include "GPUColor.h"
#include "GPURenderBundle.h"
#include "GPURenderPipeline.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderPassEncoder : public m::HybridObject {
public:
  explicit GPURenderPassEncoder(wgpu::RenderPassEncoder instance,
                                std::string label)
      : HybridObject("GPURenderPassEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  void setViewport(double x, double y, double width, double height,
                   double minDepth, double maxDepth);
  void setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
  void setBlendConstant(std::shared_ptr<GPUColor> color);
  void setStencilReference(uint32_t reference);
  void beginOcclusionQuery(uint32_t queryIndex);
  void endOcclusionQuery();
  void executeBundles(std::vector<std::shared_ptr<GPURenderBundle>> bundles);
  void end();
  void pushDebugGroup(std::string groupLabel);
  void popDebugGroup();
  void insertDebugMarker(std::string markerLabel);
  void setBindGroup(
      uint32_t index,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
      std::optional<std::vector<uint32_t>> dynamicOffsets);
  void setPipeline(std::shared_ptr<GPURenderPipeline> pipeline);
  void setIndexBuffer(std::shared_ptr<GPUBuffer> buffer,
                      wgpu::IndexFormat indexFormat,
                      std::optional<uint64_t> offset,
                      std::optional<uint64_t> size);
  void setVertexBuffer(
      uint32_t slot,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBuffer>> buffer,
      std::optional<uint64_t> offset, std::optional<uint64_t> size);
  void draw(uint32_t vertexCount, std::optional<uint32_t> instanceCount,
            std::optional<uint32_t> firstVertex,
            std::optional<uint32_t> firstInstance);
  void drawIndexed(uint32_t indexCount, std::optional<uint32_t> instanceCount,
                   std::optional<uint32_t> firstIndex,
                   std::optional<double> baseVertex,
                   std::optional<uint32_t> firstInstance);
  void drawIndirect(std::shared_ptr<GPUBuffer> indirectBuffer,
                    uint64_t indirectOffset);
  void drawIndexedIndirect(std::shared_ptr<GPUBuffer> indirectBuffer,
                           uint64_t indirectOffset);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPassEncoder::getBrand, this);
    registerHybridMethod("setViewport", &GPURenderPassEncoder::setViewport,
                         this);
    registerHybridMethod("setScissorRect",
                         &GPURenderPassEncoder::setScissorRect, this);
    registerHybridMethod("setBlendConstant",
                         &GPURenderPassEncoder::setBlendConstant, this);
    registerHybridMethod("setStencilReference",
                         &GPURenderPassEncoder::setStencilReference, this);
    registerHybridMethod("beginOcclusionQuery",
                         &GPURenderPassEncoder::beginOcclusionQuery, this);
    registerHybridMethod("endOcclusionQuery",
                         &GPURenderPassEncoder::endOcclusionQuery, this);
    registerHybridMethod("executeBundles",
                         &GPURenderPassEncoder::executeBundles, this);
    registerHybridMethod("end", &GPURenderPassEncoder::end, this);
    registerHybridMethod("pushDebugGroup",
                         &GPURenderPassEncoder::pushDebugGroup, this);
    registerHybridMethod("popDebugGroup", &GPURenderPassEncoder::popDebugGroup,
                         this);
    registerHybridMethod("insertDebugMarker",
                         &GPURenderPassEncoder::insertDebugMarker, this);
    registerHybridMethod("setBindGroup", &GPURenderPassEncoder::setBindGroup,
                         this);
    registerHybridMethod("setPipeline", &GPURenderPassEncoder::setPipeline,
                         this);
    registerHybridMethod("setIndexBuffer",
                         &GPURenderPassEncoder::setIndexBuffer, this);
    registerHybridMethod("setVertexBuffer",
                         &GPURenderPassEncoder::setVertexBuffer, this);
    registerHybridMethod("draw", &GPURenderPassEncoder::draw, this);
    registerHybridMethod("drawIndexed", &GPURenderPassEncoder::drawIndexed,
                         this);
    registerHybridMethod("drawIndirect", &GPURenderPassEncoder::drawIndirect,
                         this);
    registerHybridMethod("drawIndexedIndirect",
                         &GPURenderPassEncoder::drawIndexedIndirect, this);

    registerHybridGetter("label", &GPURenderPassEncoder::getLabel, this);
    registerHybridSetter("label", &GPURenderPassEncoder::setLabel, this);
  }

  inline const wgpu::RenderPassEncoder get() { return _instance; }

private:
  wgpu::RenderPassEncoder _instance;
  std::string _label;
};

} // namespace rnwgpu
