#include "GPUCommandEncoder.h"

#include "Convertors.h"

namespace rnwgpu {

void GPUCommandEncoder::copyBufferToBuffer(
    std::shared_ptr<GPUBuffer> source, uint64_t sourceOffset,
    std::shared_ptr<GPUBuffer> destination, uint64_t destinationOffset,
    uint64_t size) {
  _instance.CopyBufferToBuffer(source->get(), sourceOffset, destination->get(),
                               destinationOffset, size);
}

std::shared_ptr<GPUCommandBuffer> GPUCommandEncoder::finish(
    std::optional<std::shared_ptr<GPUCommandBufferDescriptor>> descriptor) {
  wgpu::CommandBufferDescriptor desc;
  std::string label = "";
  if (descriptor.has_value()) {
    label = descriptor.value()->label.value_or("");
    Convertor conv;
    if (!conv(desc, descriptor.value())) {
      throw std::runtime_error(
          "GPUCommandEncoder::finish(): error with GPUCommandBufferDescriptor");
    }
  }
  auto commandBuffer = _instance.Finish(&desc);
  return std::make_shared<GPUCommandBuffer>(commandBuffer, label);
}

std::shared_ptr<GPURenderPassEncoder> GPUCommandEncoder::beginRenderPass(
    std::shared_ptr<GPURenderPassDescriptor> descriptor) {
  Convertor conv;

  wgpu::RenderPassDescriptor desc{};
  wgpu::RenderPassDescriptorMaxDrawCount maxDrawCountDesc{};
  desc.nextInChain = &maxDrawCountDesc;

  // TODO: reenable
  //    if (!conv(desc.colorAttachments, desc.colorAttachmentCount,
  //    descriptor.colorAttachments) ||
  //        !conv(desc.depthStencilAttachment,
  //        descriptor.depthStencilAttachment) || !conv(desc.label,
  //        descriptor.label) || !conv(desc.occlusionQuerySet,
  //        descriptor.occlusionQuerySet) || !conv(desc.timestampWrites,
  //        descriptor.timestampWrites) || !conv(maxDrawCountDesc.maxDrawCount,
  //        descriptor.maxDrawCount)) { return {};
  //    }
  auto renderPass = _instance.BeginRenderPass(&desc);
  return std::make_shared<GPURenderPassEncoder>(renderPass,
                                                descriptor->label.value_or(""));
}

void GPUCommandEncoder::copyTextureToBuffer(
    std::shared_ptr<GPUImageCopyTexture> source,
    std::shared_ptr<GPUImageCopyBuffer> destination,
    std::shared_ptr<GPUExtent3D> copySize) {
  Convertor conv;
  wgpu::ImageCopyTexture src{};
  wgpu::ImageCopyBuffer dst{};
  wgpu::Extent3D size{};
  if (!conv(src, source) ||      //
      !conv(dst, destination) || //
      !conv(size, copySize)) {
    return;
  }
  _instance.CopyTextureToBuffer(&src, &dst, &size);
}

std::shared_ptr<GPUComputePassEncoder> GPUCommandEncoder::beginComputePass(
    std::shared_ptr<GPUComputePassDescriptor> descriptor) {
  wgpu::ComputePassDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto computePass = _instance.BeginComputePass(&desc);
  return std::make_shared<GPUComputePassEncoder>(
      computePass, descriptor->label.value_or(""));
}

} // namespace rnwgpu
