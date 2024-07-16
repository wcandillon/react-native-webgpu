#include "GPURenderBundleEncoder.h"

namespace rnwgpu {

std::shared_ptr<GPURenderBundle> GPURenderBundleEncoder::finish(
    std::shared_ptr<GPURenderBundleDescriptor> descriptor) {
  auto bundle = _instance.Finish(descriptor->getInstance());
  return std::make_shared<GPURenderBundle>(bundle, descriptor->label);
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
    uint32_t groupIndex, std::shared_ptr<GPUBindGroup> group,
    size_t dynamicOffsetCount,
    std::optional<std::vector<uint32_t>> dynamicOffsets) {

  if (dynamicOffsetCount == 0) {
    _instance.SetBindGroup(groupIndex, group->get(), 0, nullptr);
  } else {
    _instance.SetBindGroup(groupIndex, group->get(), dynamicOffsetCount,
                           dynamicOffsets->data());
  }
}

} // namespace rnwgpu