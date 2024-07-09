#include "GPUBuffer.h"

namespace rnwgpu {

std::shared_ptr<MutableJSIBuffer>
GPUBuffer::getMappedRange(std::optional<double> offset,
                          std::optional<double> size) {
  auto aOffset = offset.value_or(0);
  auto aSize = size.value_or(WGPU_WHOLE_MAP_SIZE);
  auto result = _instance->GetMappedRange(aOffset, aSize);
  return std::make_shared<MutableJSIBuffer>(result, _instance->GetSize());
};

void GPUBuffer::unmap() { _instance->Unmap(); };

} // namespace rnwgpu