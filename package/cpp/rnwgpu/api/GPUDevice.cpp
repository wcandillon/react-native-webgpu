#include "GPUDevice.h"

#include "Convertors.h"

namespace rnwgpu {

std::shared_ptr<GPUBuffer>
GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
  wgpu::BufferDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error(
        "GPUDevice::createBuffer(): Error with GPUBufferDescriptor");
  }
  auto result = _instance.CreateBuffer(&desc);
  return std::make_shared<GPUBuffer>(result, _async,
                                     descriptor->label.value_or(""));
}

std::shared_ptr<GPUQueue> GPUDevice::getQueue() {
  auto result = _instance.GetQueue();
  return std::make_shared<GPUQueue>(result, _async, _label);
}

std::shared_ptr<GPUCommandEncoder> GPUDevice::createCommandEncoder(
    std::optional<std::shared_ptr<GPUCommandEncoderDescriptor>> descriptor) {
  wgpu::CommandEncoderDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUCommandEncoderDescriptor");
  }
  auto result = _instance.CreateCommandEncoder(&desc);
  return std::make_shared<GPUCommandEncoder>(
      result,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

void GPUDevice::destroy() { _instance.Destroy(); }

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  wgpu::TextureDescriptor desc;
  // TODO: implement
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUTextureDescriptor");
  }
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
  wgpu::RenderPipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPURenderPipelineDescriptor");
  }
  assert(desc.fragment != nullptr && "Fragment state must not be null");
  auto renderPipeline = _instance.CreateRenderPipeline(&desc);
  return std::make_shared<GPURenderPipeline>(renderPipeline,
                                             descriptor->label.value_or(""));
}

std::shared_ptr<GPUBindGroup>
GPUDevice::createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor) {
  Convertor conv;
  wgpu::BindGroupDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.layout, descriptor->layout) ||
      !conv(desc.entries, desc.entryCount, descriptor->entries)) {
    throw std::runtime_error(
        "GPUBindGroup::createBindGroup(): Error with GPUBindGroupDescriptor");
  }
  auto bindGroup = _instance.CreateBindGroup(&desc);
  return std::make_shared<GPUBindGroup>(bindGroup,
                                        descriptor->label.value_or(""));
}

std::shared_ptr<GPUSampler> GPUDevice::createSampler(
    std::optional<std::shared_ptr<GPUSamplerDescriptor>> descriptor) {
  wgpu::SamplerDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createSampler(): Error with "
                             "GPUSamplerDescriptor");
  }
  auto sampler = _instance.CreateSampler(&desc);
  return std::make_shared<GPUSampler>(
      sampler,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

std::shared_ptr<GPUComputePipeline> GPUDevice::createComputePipeline(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                             "GPUComputePipelineDescriptor");
  }
  auto computePipeline = _instance.CreateComputePipeline(&desc);
  return std::make_shared<GPUComputePipeline>(computePipeline,
                                              descriptor->label.value_or(""));
}

std::shared_ptr<GPUQuerySet>
GPUDevice::createQuerySet(std::shared_ptr<GPUQuerySetDescriptor> descriptor) {
  wgpu::QuerySetDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createQuerySet(): Error with "
                             "GPUQuerySetDescriptor");
  }
  auto querySet = _instance.CreateQuerySet(&desc);
  return std::make_shared<GPUQuerySet>(querySet,
                                       descriptor->label.value_or(""));
}

std::shared_ptr<GPURenderBundleEncoder> GPUDevice::createRenderBundleEncoder(
    std::shared_ptr<GPURenderBundleEncoderDescriptor> descriptor) {
  Convertor conv;

  wgpu::RenderBundleEncoderDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.colorFormats, desc.colorFormatCount,
            descriptor->colorFormats) ||
      !conv(desc.depthStencilFormat, descriptor->depthStencilFormat) ||
      !conv(desc.sampleCount, descriptor->sampleCount) ||
      !conv(desc.depthReadOnly, descriptor->depthReadOnly) ||
      !conv(desc.stencilReadOnly, descriptor->stencilReadOnly)) {
    return {};
  }
  return std::make_shared<GPURenderBundleEncoder>(
      _instance.CreateRenderBundleEncoder(&desc),
      descriptor->label.value_or(""));
}
} // namespace rnwgpu
