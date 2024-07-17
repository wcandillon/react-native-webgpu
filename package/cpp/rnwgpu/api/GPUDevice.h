#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"
#include "Convertors.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandEncoder.h"
#include "GPUCommandEncoderDescriptor.h"
#include "GPUComputePipeline.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUQueue.h"
#include "GPURenderPipeline.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  explicit GPUDevice(wgpu::Device instance, std::shared_ptr<AsyncRunner> async,
                     std::string label)
      : HybridObject("GPUDevice"), _instance(instance), _async(async),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  void destroy();
  std::shared_ptr<GPUBuffer>
  createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor);
  std::shared_ptr<GPUTexture>
  createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor);
  std::shared_ptr<GPUSampler>
  createSampler(std::shared_ptr<GPUSamplerDescriptor> descriptor);
  std::shared_ptr<GPUBindGroup>
  createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor);
  std::shared_ptr<GPUShaderModule>
  createShaderModule(std::shared_ptr<GPUShaderModuleDescriptor> descriptor);
  std::shared_ptr<GPUComputePipeline> createComputePipeline(
      std::shared_ptr<GPUComputePipelineDescriptor> descriptor);
  std::shared_ptr<GPURenderPipeline>
  createRenderPipeline(std::shared_ptr<GPURenderPipelineDescriptor> descriptor);
  std::shared_ptr<GPUCommandEncoder>
  createCommandEncoder(std::shared_ptr<GPUCommandEncoderDescriptor> descriptor);

  std::shared_ptr<GPUQueue> getQueue();

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDevice::getBrand, this);
    registerHybridMethod("destroy", &GPUDevice::destroy, this);
    registerHybridMethod("createBuffer", &GPUDevice::createBuffer, this);
    registerHybridMethod("createTexture", &GPUDevice::createTexture, this);
    registerHybridMethod("createSampler", &GPUDevice::createSampler, this);
    registerHybridMethod("createBindGroup", &GPUDevice::createBindGroup, this);
    registerHybridMethod("createShaderModule", &GPUDevice::createShaderModule,
                         this);
    registerHybridMethod("createComputePipeline",
                         &GPUDevice::createComputePipeline, this);
    registerHybridMethod("createRenderPipeline",
                         &GPUDevice::createRenderPipeline, this);
    registerHybridMethod("createCommandEncoder",
                         &GPUDevice::createCommandEncoder, this);
    registerHybridGetter("queue", &GPUDevice::getQueue, this);
    registerHybridGetter("label", &GPUDevice::getLabel, this);
  }

  inline const wgpu::Device get() { return _instance; }

private:
  wgpu::Device _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
};
} // namespace rnwgpu