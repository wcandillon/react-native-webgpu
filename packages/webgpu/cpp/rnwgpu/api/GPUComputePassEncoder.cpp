#include "GPUComputePassEncoder.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <string>

namespace rnwgpu {

void GPUComputePassEncoder::setPipeline(
    std::shared_ptr<GPUComputePipeline> pipeline) {
  _instance.SetPipeline(pipeline->get());
  trackCommand(kSmallCommandCost);
}

void GPUComputePassEncoder::end() {
  if (_ended) {
    return;
  }
  _instance.End();
  flushToOwner();
  _ended = true;
  _estimatedCommandBytes = kEndedResidual;
}

void GPUComputePassEncoder::setBindGroup(
    uint32_t index,
    std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
    std::optional<std::vector<uint32_t>> dynamicOffsets) {
  auto dynOffsets = dynamicOffsets.value_or(std::vector<uint32_t>());
  if (dynOffsets.size() == 0) {
    if (std::holds_alternative<std::nullptr_t>(bindGroup)) {
      _instance.SetBindGroup(index, nullptr, 0, nullptr);
    } else {
      auto group = std::get<std::shared_ptr<GPUBindGroup>>(bindGroup);
      _instance.SetBindGroup(index, group->get(), 0, nullptr);
    }
  } else {
    if (std::holds_alternative<std::nullptr_t>(bindGroup)) {
      _instance.SetBindGroup(index, nullptr, dynOffsets.size(),
                             dynamicOffsets->data());
    } else {
      auto group = std::get<std::shared_ptr<GPUBindGroup>>(bindGroup);
      _instance.SetBindGroup(index, group->get(), dynOffsets.size(),
                             dynamicOffsets->data());
    }
  }
  size_t dynamicOffsetCost =
      dynOffsets.size() > 0 ? dynOffsets.size() * 32 : 0;
  trackCommand(kSmallCommandCost + dynamicOffsetCost);
}

void GPUComputePassEncoder::dispatchWorkgroups(
    uint32_t workgroupCountX, std::optional<uint32_t> workgroupCountY,
    std::optional<uint32_t> workgroupCountZ) {
  _instance.DispatchWorkgroups(workgroupCountX, workgroupCountY.value_or(1),
                               workgroupCountZ.value_or(1));
  trackCommand(kDispatchCommandCost);
}

void GPUComputePassEncoder::dispatchWorkgroupsIndirect(
    std::shared_ptr<GPUBuffer> indirectBuffer, uint64_t indirectOffset) {
  _instance.DispatchWorkgroupsIndirect(indirectBuffer->get(), indirectOffset);
  trackCommand(kDispatchCommandCost);
}

void GPUComputePassEncoder::pushDebugGroup(std::string groupLabel) {
  _instance.PushDebugGroup(groupLabel.c_str());
  trackCommand(kDebugCommandCost);
}

void GPUComputePassEncoder::popDebugGroup() {
  _instance.PopDebugGroup();
  trackCommand(kDebugCommandCost);
}

void GPUComputePassEncoder::insertDebugMarker(std::string markerLabel) {
  _instance.InsertDebugMarker(markerLabel.c_str());
  trackCommand(kDebugCommandCost);
}

void GPUComputePassEncoder::trackCommand(size_t bytes) {
  if (_ended) {
    return;
  }
  const size_t headroom =
      std::numeric_limits<size_t>::max() - _estimatedCommandBytes;
  _estimatedCommandBytes += std::min(bytes, headroom);
}

void GPUComputePassEncoder::flushToOwner() {
  if (_byteSink) {
    _byteSink(_estimatedCommandBytes);
    _byteSink = nullptr;
  }
}

} // namespace rnwgpu
