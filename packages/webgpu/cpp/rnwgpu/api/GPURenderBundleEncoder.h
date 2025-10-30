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
#include "GPURenderBundle.h"
#include "GPURenderBundleDescriptor.h"
#include "GPURenderPipeline.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundleEncoder : public m::HybridObject {
public:
  explicit GPURenderBundleEncoder(wgpu::RenderBundleEncoder instance,
                                  std::string label)
      : HybridObject("GPURenderBundleEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPURenderBundle>
  finish(std::optional<std::shared_ptr<GPURenderBundleDescriptor>> descriptor);
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
    registerHybridGetter("__brand", &GPURenderBundleEncoder::getBrand, this);
    registerHybridMethod("finish", &GPURenderBundleEncoder::finish, this);
    registerHybridMethod("pushDebugGroup",
                         &GPURenderBundleEncoder::pushDebugGroup, this);
    registerHybridMethod("popDebugGroup",
                         &GPURenderBundleEncoder::popDebugGroup, this);
    registerHybridMethod("insertDebugMarker",
                         &GPURenderBundleEncoder::insertDebugMarker, this);
    registerHybridMethod("setBindGroup", &GPURenderBundleEncoder::setBindGroup,
                         this);
    registerHybridMethod("setPipeline", &GPURenderBundleEncoder::setPipeline,
                         this);
    registerHybridMethod("setIndexBuffer",
                         &GPURenderBundleEncoder::setIndexBuffer, this);
    registerHybridMethod("setVertexBuffer",
                         &GPURenderBundleEncoder::setVertexBuffer, this);
    registerHybridMethod("draw", &GPURenderBundleEncoder::draw, this);
    registerHybridMethod("drawIndexed", &GPURenderBundleEncoder::drawIndexed,
                         this);
    registerHybridMethod("drawIndirect", &GPURenderBundleEncoder::drawIndirect,
                         this);
    registerHybridMethod("drawIndexedIndirect",
                         &GPURenderBundleEncoder::drawIndexedIndirect, this);

    registerHybridGetter("label", &GPURenderBundleEncoder::getLabel, this);
    registerHybridSetter("label", &GPURenderBundleEncoder::setLabel, this);
  }

  inline const wgpu::RenderBundleEncoder get() { return _instance; }

  size_t getMemoryPressure() override { return _estimatedCommandBytes; }

private:
  void trackCommand(size_t bytes);
  static constexpr size_t kBaseEncoderBytes = 6 * 1024;
  static constexpr size_t kDrawCommandCost = 512;
  static constexpr size_t kSmallCommandCost = 160;
  static constexpr size_t kDebugCommandCost = 96;
  static constexpr size_t kIndirectCommandCost = 768;
  static constexpr size_t kFinishResidualCost = 512;
  wgpu::RenderBundleEncoder _instance;
  std::string _label;
  size_t _estimatedCommandBytes = kBaseEncoderBytes;
  bool _finished = false;
};

} // namespace rnwgpu
