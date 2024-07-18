#include "GPUDevice.h"

namespace rnwgpu {

std::shared_ptr<GPUBuffer>
GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
  wgpu::BufferDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto result = _instance.CreateBuffer(&desc);
  return std::make_shared<GPUBuffer>(result, _async,
                                     descriptor->label.value_or(""));
}

std::shared_ptr<GPUQueue> GPUDevice::getQueue() {
  auto result = _instance.GetQueue();
  return std::make_shared<GPUQueue>(result, _async, _label);
}

std::shared_ptr<GPUCommandEncoder> GPUDevice::createCommandEncoder(
    std::shared_ptr<GPUCommandEncoderDescriptor> descriptor) {
  wgpu::CommandEncoderDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto result = _instance.CreateCommandEncoder(&desc);
  return std::make_shared<GPUCommandEncoder>(result,
                                             descriptor->label.value_or(""));
}

void GPUDevice::destroy() { _instance.Destroy(); }

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  wgpu::TextureDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture, descriptor->label.value_or(""));
}

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderModuleWGSLDescriptor wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  wgsl_desc.code = descriptor->code.c_str();
  sm_desc.label = descriptor->label.value_or("").c_str();
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    return std::make_shared<GPUShaderModule>(
        _instance.CreateErrorShaderModule(
            &sm_desc, "The WGSL shader contains an illegal character '\\0'"),
        _async, descriptor->label.value_or(""));
  }
  auto module = _instance.CreateShaderModule(&sm_desc);
  return std::make_shared<GPUShaderModule>(module, _async,
                                           descriptor->label.value_or(""));
}

std::shared_ptr<GPURenderPipeline> GPUDevice::createRenderPipeline(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  wgpu::RenderPipelineDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto renderPipeline = _instance.CreateRenderPipeline(&desc);
  return std::make_shared<GPURenderPipeline>(renderPipeline,
                                             descriptor->label.value_or(""));
}

std::shared_ptr<GPUBindGroup>
GPUDevice::createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor) {
  wgpu::BindGroupDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto bindGroup = _instance.CreateBindGroup(&desc);
  return std::make_shared<GPUBindGroup>(bindGroup,
                                        descriptor->label.value_or(""));
}

std::shared_ptr<GPUSampler>
GPUDevice::createSampler(std::shared_ptr<GPUSamplerDescriptor> descriptor) {
  wgpu::SamplerDescriptor desc;
  auto sampler = _instance.CreateSampler(&desc);
  return std::make_shared<GPUSampler>(sampler, descriptor->label.value_or(""));
}

std::shared_ptr<GPUComputePipeline> GPUDevice::createComputePipeline(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc;
  Convertor conv;
  conv(desc, descriptor);
  auto computePipeline = _instance.CreateComputePipeline(&desc);
  return std::make_shared<GPUComputePipeline>(computePipeline,
                                              descriptor->label.value_or(""));
}

} // namespace rnwgpu
