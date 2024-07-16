#include "GPUDevice.h"

namespace rnwgpu {

std::shared_ptr<GPUBuffer>
GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
  auto aDescriptor = descriptor->getInstance();
  auto result = _instance.CreateBuffer(aDescriptor);
  return std::make_shared<GPUBuffer>(result, _async, descriptor->label);
}

std::shared_ptr<GPUQueue> GPUDevice::getQueue() {
  auto result = _instance.GetQueue();
  return std::make_shared<GPUQueue>(result, _async, _label);
}

std::shared_ptr<GPUCommandEncoder> GPUDevice::createCommandEncoder(
    std::shared_ptr<GPUCommandEncoderDescriptor> descriptor) {
  auto aDescriptor = descriptor->getInstance();
  auto result = _instance.CreateCommandEncoder(aDescriptor);
  return std::make_shared<GPUCommandEncoder>(result, descriptor->label);
}

void GPUDevice::destroy() { _instance.Destroy(); }

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  auto texture = _instance.CreateTexture(descriptor->getInstance());
  return std::make_shared<GPUTexture>(texture, descriptor->label);
}

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderModuleWGSLDescriptor wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  wgsl_desc.code = descriptor->code.c_str();
  sm_desc.label = descriptor->_instance.label;
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    return std::make_shared<GPUShaderModule>(
        _instance.CreateErrorShaderModule(
            &sm_desc, "The WGSL shader contains an illegal character '\\0'"),
        _async, descriptor->label);
  }
  auto module = _instance.CreateShaderModule(&sm_desc);
  return std::make_shared<GPUShaderModule>(module, _async, descriptor->label);
}

std::shared_ptr<GPURenderPipeline> GPUDevice::createRenderPipeline(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  auto renderPipeline =
      _instance.CreateRenderPipeline(descriptor->getInstance());
  return std::make_shared<GPURenderPipeline>(renderPipeline, descriptor->label);
}

std::shared_ptr<GPUBindGroup>
GPUDevice::createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor) {
  auto bindGroup = _instance.CreateBindGroup(descriptor->getInstance());
  return std::make_shared<GPUBindGroup>(bindGroup, descriptor->label);
}

std::shared_ptr<GPUSampler>
GPUDevice::createSampler(std::shared_ptr<GPUSamplerDescriptor> descriptor) {
  auto sampler = _instance.CreateSampler(descriptor->getInstance());
  return std::make_shared<GPUSampler>(sampler, descriptor->label);
}

std::shared_ptr<GPUComputePipeline> GPUDevice::createComputePipeline(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  auto computePipeline =
      _instance.CreateComputePipeline(descriptor->getInstance());
  return std::make_shared<GPUComputePipeline>(computePipeline,
                                              descriptor->label);
}

} // namespace rnwgpu
