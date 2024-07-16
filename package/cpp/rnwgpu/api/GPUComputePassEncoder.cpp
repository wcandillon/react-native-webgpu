#include "GPUComputePassEncoder.h"

namespace rnwgpu {

void GPUComputePassEncoder::setPipeline(
    std::shared_ptr<GPUComputePipeline> pipeline) {
  _instance.SetPipeline(pipeline->get());
}

void GPUComputePassEncoder::end() { _instance.End(); }

void GPUComputePassEncoder::setBindGroup(
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