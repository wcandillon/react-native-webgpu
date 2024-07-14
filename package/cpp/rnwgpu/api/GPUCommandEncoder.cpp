#include "GPUCommandEncoder.h"

namespace rnwgpu {

void GPUCommandEncoder::copyBufferToBuffer(
    std::shared_ptr<GPUBuffer> source, uint64_t sourceOffset,
    std::shared_ptr<GPUBuffer> destination, uint64_t destinationOffset,
    uint64_t size) {

  _instance.CopyBufferToBuffer(source->get(), sourceOffset, destination->get(),
                               destinationOffset, size);
}

std::shared_ptr<GPUCommandBuffer> GPUCommandEncoder::finish(
    std::shared_ptr<GPUCommandBufferDescriptor> descriptor) {
  auto commandBuffer = _instance.Finish(&descriptor->_instance);
  return std::make_shared<GPUCommandBuffer>(commandBuffer, _label);
}

} // namespace rnwgpu
