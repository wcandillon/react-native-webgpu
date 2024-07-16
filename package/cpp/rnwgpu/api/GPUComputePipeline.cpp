#include "GPUComputePipeline.h"

namespace rnwgpu {

std::shared_ptr<GPUBindGroupLayout>
GPUComputePipeline::getBindGroupLayout(uint32_t groupIndex) {
  auto bindGroup = _instance.GetBindGroupLayout(groupIndex);
  // TODO: the label is not correct here
  return std::make_shared<GPUBindGroupLayout>(bindGroup, _label);
}

} // namespace rnwgpu