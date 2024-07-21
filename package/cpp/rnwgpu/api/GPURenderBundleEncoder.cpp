#include "GPURenderBundleEncoder.h"

#include "Convertors.h"

namespace rnwgpu {

std::shared_ptr<GPURenderBundle> GPURenderBundleEncoder::finish(
    std::optional<std::shared_ptr<GPURenderBundleDescriptor>> descriptor) {
  wgpu::RenderBundleDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPURenderBundleEncoder.finish(): couldn't get "
                             "GPURenderBundleDescriptor");
  }
  auto bundle = _instance.Finish(&desc);
  return std::make_shared<GPURenderBundle>(
      bundle,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

void GPURenderBundleEncoder::setPipeline(
    std::shared_ptr<GPURenderPipeline> pipeline) {
  _instance.SetPipeline(pipeline->get());
}

void GPURenderBundleEncoder::draw(uint32_t vertexCount,
                                  std::optional<uint32_t> instanceCount,
                                  std::optional<uint32_t> firstVertex,
                                  std::optional<uint32_t> firstInstance) {
  _instance.Draw(vertexCount, instanceCount.value_or(1),
                 firstVertex.value_or(0), firstInstance.value_or(0));
}

void GPURenderBundleEncoder::setBindGroup(
      uint32_t groupIndex,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
      std::optional<std::vector<uint32_t>> dynamicOffsets) {
  auto dynOffsets = dynamicOffsets.value_or(std::vector<uint32_t>());
  if (dynOffsets.size() == 0) {
    if (std::holds_alternative<std::nullptr_t>(bindGroup)) {
      _instance.SetBindGroup(groupIndex, nullptr, 0, nullptr);
    } else {
      auto group = std::get<std::shared_ptr<GPUBindGroup>>(bindGroup);
      _instance.SetBindGroup(groupIndex, group->get(), 0, nullptr);
    }
  } else {
        if (std::holds_alternative<std::nullptr_t>(bindGroup)) {
    _instance.SetBindGroup(groupIndex, nullptr, dynOffsets.size(),
                           dynamicOffsets->data());
    } else {
      auto group = std::get<std::shared_ptr<GPUBindGroup>>(bindGroup);
    _instance.SetBindGroup(groupIndex, group->get(), dynOffsets.size(),
                           dynamicOffsets->data());
    }
  }
}

} // namespace rnwgpu
