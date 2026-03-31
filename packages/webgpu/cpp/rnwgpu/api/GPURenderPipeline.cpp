#include "GPURenderPipeline.h"

#include <memory>

namespace rnwgpu {

std::shared_ptr<GPUBindGroupLayout>
GPURenderPipeline::getBindGroupLayout(uint32_t groupIndex) {
  auto bindGroupLayout = _instance.GetBindGroupLayout(groupIndex);
  auto result = std::make_shared<GPUBindGroupLayout>(bindGroupLayout, "");
  result->setGPULock(getGPULock());
  return result;
}

} // namespace rnwgpu