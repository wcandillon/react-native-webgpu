#include "GPUComputePipeline.h"
#include <memory>

namespace rnwgpu {

std::shared_ptr<GPUBindGroupLayout>
GPUComputePipeline::getBindGroupLayout(uint32_t groupIndex) {
  auto bindGroup = _instance.GetBindGroupLayout(groupIndex);
  auto result = std::make_shared<GPUBindGroupLayout>(bindGroup, "");
  result->setGPULock(getGPULock());
  return result;
}

} // namespace rnwgpu