#include "GPUQueue.h"

namespace rnwgpu {

void GPUQueue::writeBuffer(std::shared_ptr<GPUBuffer> buffer,
                           uint64_t bufferOffset,
                           std::shared_ptr<MutableJSIBuffer> data,
                           std::optional<uint64_t> dataOffset,
                           std::optional<size_t> size) {}

} // namespace rnwgpu
