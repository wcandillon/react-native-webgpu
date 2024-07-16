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

std::shared_ptr<GPURenderPassEncoder> GPUCommandEncoder::beginRenderPass(
    std::shared_ptr<GPURenderPassDescriptor> descriptor) {
  auto renderPass = _instance.BeginRenderPass(&descriptor->_instance);
  return std::make_shared<GPURenderPassEncoder>(renderPass, descriptor->label);
}

void GPUCommandEncoder::copyTextureToBuffer(
    std::shared_ptr<GPUImageCopyTexture> source,
    std::shared_ptr<GPUImageCopyBuffer> destination,
    std::shared_ptr<GPUExtent3D> copySize) {
  auto src = source->getInstance();
  auto dst = destination->getInstance();
  auto size = copySize->getInstance();
  _instance.CopyTextureToBuffer(src, dst, size);
}

std::shared_ptr<GPUComputePassEncoder> GPUCommandEncoder::beginComputePass(
    std::shared_ptr<GPUComputePassDescriptor> descriptor) {
  auto computePass = _instance.BeginComputePass(&descriptor->_instance);
  return std::make_shared<GPUComputePassEncoder>(computePass,
                                                 descriptor->label);
}

} // namespace rnwgpu
