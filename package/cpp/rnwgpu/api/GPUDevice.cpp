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

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderModuleWGSLDescriptor wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  wgsl_desc.code = descriptor->code.c_str();
  sm_desc.label = descriptor->_instance.label;
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    // return interop::GPUShaderModule::Create<GPUShaderModule>(
    //     env, sm_desc,
    //     device_.CreateErrorShaderModule(&sm_desc,
    //                                     "The WGSL shader contains an illegal
    //                                     character '\\0'"),
    //     async_);
    throw std::runtime_error(
        "The WGSL shader contains an illegal character '\\0'");
  }
  auto module = _instance.CreateShaderModule(&sm_desc);
  return std::make_shared<GPUShaderModule>(module, _async, descriptor->label);
}

} // namespace rnwgpu
