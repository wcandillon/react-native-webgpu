#include "GPUCommandEncoder.h"

namespace rnwgpu {

void GPUCommandEncoder::copyBufferToBuffer(
    std::shared_ptr<GPUBuffer> source, uint64_t sourceOffset,
    std::shared_ptr<GPUBuffer> destination, uint64_t destinationOffset,
    uint64_t size) {

  _instance.CopyBufferToBuffer(source->get(), sourceOffset,
                               destination->get(), destinationOffset, size);
}

} // namespace rnwgpu
