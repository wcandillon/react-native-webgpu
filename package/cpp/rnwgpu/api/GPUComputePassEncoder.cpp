#include "GPUComputePassEncoder.h"

namespace rnwgpu {

void GPUComputePassEncoder::setPipeline(
    std::shared_ptr<GPUComputePipeline> pipeline) {
  _instance.SetPipeline(pipeline->get());
}

void GPUComputePassEncoder::end() { _instance.End(); }

void GPUComputePassEncoder::setBindGroup(
    uint32_t groupIndex, std::shared_ptr<GPUBindGroup> group,
    std::optional<std::vector<uint32_t>> dynamicOffsets) {
  auto dynOffsets = dynamicOffsets.value_or(std::vector<uint32_t>());
  if (dynOffsets.size() == 0) {
    _instance.SetBindGroup(groupIndex, group->get(), 0, nullptr);
  } else {
    _instance.SetBindGroup(groupIndex, group->get(), dynOffsets.size(),
                           dynamicOffsets->data());
  }
}

void GPUComputePassEncoder::dispatchWorkgroups(
    uint32_t workgroupCountX, std::optional<uint32_t> workgroupCountY,
    std::optional<uint32_t> workgroupCountZ) {
  _instance.DispatchWorkgroups(workgroupCountX, workgroupCountY.value_or(1),
                               workgroupCountZ.value_or(1));
}

} // namespace rnwgpu