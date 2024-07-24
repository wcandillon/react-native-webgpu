#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
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

  void beginOcclusionQuery(uint32_t queryIndex);
  void endOcclusionQuery();
  void executeBundles(std::vector<std::shared_ptr<GPURenderBundle>> bundles);
  void end();
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

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPassEncoder::getBrand, this);
    registerHybridMethod("beginOcclusionQuery",
                         &GPURenderPassEncoder::beginOcclusionQuery, this);
    registerHybridMethod("endOcclusionQuery",
                         &GPURenderPassEncoder::endOcclusionQuery, this);
    registerHybridMethod("executeBundles",
                         &GPURenderPassEncoder::executeBundles, this);
    registerHybridMethod("end", &GPURenderPassEncoder::end, this);
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

    registerHybridGetter("label", &GPURenderPassEncoder::getLabel, this);
  }

  inline const wgpu::RenderPassEncoder get() { return _instance; }

private:
  wgpu::RenderPassEncoder _instance;
  std::string _label;
};

} // namespace rnwgpu