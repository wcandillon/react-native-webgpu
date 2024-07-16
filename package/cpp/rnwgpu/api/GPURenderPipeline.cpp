#include "GPURenderPipeline.h"

#include <memory>

namespace rnwgpu {

std::shared_ptr<GPUBindGroupLayout> GPURenderPipeline::getBindGroupLayout(uint32_t groupIndex) {
  auto bindGroupLayout = _instance.GetBindGroupLayout(groupIndex);
  // TODO: handle label properly here
  return std::make_shared<GPUBindGroupLayout>(bindGroupLayout, "");
}

} // namespace rnwgpu