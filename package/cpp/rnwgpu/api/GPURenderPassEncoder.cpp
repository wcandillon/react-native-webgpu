#include "GPURenderPassEncoder.h"
#include "Convertors.h"

namespace rnwgpu {

void GPURenderPassEncoder::end() { _instance.End(); }

void GPURenderPassEncoder::setPipeline(
    std::shared_ptr<GPURenderPipeline> pipeline) {
  _instance.SetPipeline(pipeline->get());
}

void GPURenderPassEncoder::draw(uint32_t vertexCount,
                                std::optional<uint32_t> instanceCount,
                                std::optional<uint32_t> firstVertex,
                                std::optional<uint32_t> firstInstance) {
  _instance.Draw(vertexCount, instanceCount.value_or(1),
                 firstVertex.value_or(0), firstInstance.value_or(0));
}
void GPURenderPassEncoder::setVertexBuffer(
    uint32_t slot,
    std::variant<std::nullptr_t, std::shared_ptr<GPUBuffer>> buffer,
    std::optional<uint64_t> offset, std::optional<uint64_t> size) {
  Convertor conv;

  wgpu::Buffer b{};
  uint64_t s = wgpu::kWholeSize;
  if (!conv(b, buffer) || !conv(s, size)) {
    return;
  }
  _instance.SetVertexBuffer(slot, b, offset.value_or(0), s);
}

void GPURenderPassEncoder::setBindGroup(
    uint32_t groupIndex,
    std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
    std::optional<std::vector<uint32_t>> dynamicOffsets) {
  auto dynOffsets = dynamicOffsets.value_or(std::vector<uint32_t>());
  if (dynOffsets.size() == 0) {
    if (std::holds_alternative<nullptr_t>(bindGroup)) {
      _instance.SetBindGroup(groupIndex, nullptr, 0, nullptr);
    } else {
      auto group = std::get<std::shared_ptr<GPUBindGroup>>(bindGroup);
      _instance.SetBindGroup(groupIndex, group->get(), 0, nullptr);
    }
  } else {
    if (std::holds_alternative<nullptr_t>(bindGroup)) {
      _instance.SetBindGroup(groupIndex, nullptr, dynOffsets.size(),
                             dynamicOffsets->data());
    } else {
      auto group = std::get<std::shared_ptr<GPUBindGroup>>(bindGroup);
      _instance.SetBindGroup(groupIndex, group->get(), dynOffsets.size(),
                             dynamicOffsets->data());
    }
  }
}

void GPURenderPassEncoder::setIndexBuffer(std::shared_ptr<GPUBuffer> buffer,
                                          wgpu::IndexFormat indexFormat,
                                          std::optional<uint64_t> offset,
                                          std::optional<uint64_t> size) {
  Convertor conv;

  wgpu::Buffer b{};
  wgpu::IndexFormat f{};
  uint64_t o = 0;
  uint64_t s = wgpu::kWholeSize;
  if (!conv(b, buffer) ||      //
      !conv(f, indexFormat) || //
      !conv(o, offset) ||      //
      !conv(s, size)) {
    return;
  }

  _instance.SetIndexBuffer(b, f, o, s);
}

} // namespace rnwgpu