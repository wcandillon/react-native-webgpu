#pragma once

#include <future>
#include <memory>
#include <string>
#include <unordered_set>
#include <variant>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBindGroupLayout.h"
#include "GPUBindGroupLayoutDescriptor.h"
#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandEncoder.h"
#include "GPUCommandEncoderDescriptor.h"
#include "GPUComputePipeline.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUDeviceLostInfo.h"
#include "GPUError.h"
#include "GPUExternalTexture.h"
#include "GPUExternalTextureDescriptor.h"
#include "GPUPipelineLayout.h"
#include "GPUPipelineLayoutDescriptor.h"
#include "GPUQuerySet.h"
#include "GPUQuerySetDescriptor.h"
#include "GPUQueue.h"
#include "GPURenderBundleEncoder.h"
#include "GPURenderBundleEncoderDescriptor.h"
#include "GPURenderPipeline.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUSupportedLimits.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  explicit GPUDevice(wgpu::Device instance, std::shared_ptr<AsyncRunner> async,
                     std::string label)
      : HybridObject("GPUDevice"), _instance(instance), _async(async),
        _label(label) {
    m_lostPromise =
        std::make_shared<std::promise<std::shared_ptr<GPUDeviceLostInfo>>>();

    auto sharedFuture = m_lostPromise->get_future().share();
    m_lostSharedFuture = std::make_shared<
        std::shared_future<std::shared_ptr<GPUDeviceLostInfo>>>(sharedFuture);
  }

public:
  std::string getBrand() { return _name; }

  void destroy();
  std::shared_ptr<GPUBuffer>
  createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor);
  std::shared_ptr<GPUTexture>
  createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor);
  std::shared_ptr<GPUSampler> createSampler(
      std::optional<std::shared_ptr<GPUSamplerDescriptor>> descriptor);
  std::shared_ptr<GPUExternalTexture> importExternalTexture(
      std::shared_ptr<GPUExternalTextureDescriptor> descriptor);
  std::shared_ptr<GPUBindGroupLayout> createBindGroupLayout(
      std::shared_ptr<GPUBindGroupLayoutDescriptor> descriptor);
  std::shared_ptr<GPUPipelineLayout>
  createPipelineLayout(std::shared_ptr<GPUPipelineLayoutDescriptor> descriptor);
  std::shared_ptr<GPUBindGroup>
  createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor);
  std::shared_ptr<GPUShaderModule>
  createShaderModule(std::shared_ptr<GPUShaderModuleDescriptor> descriptor);
  std::shared_ptr<GPUComputePipeline> createComputePipeline(
      std::shared_ptr<GPUComputePipelineDescriptor> descriptor);
  std::shared_ptr<GPURenderPipeline>
  createRenderPipeline(std::shared_ptr<GPURenderPipelineDescriptor> descriptor);
  std::future<std::shared_ptr<GPUComputePipeline>> createComputePipelineAsync(
      std::shared_ptr<GPUComputePipelineDescriptor> descriptor);
  std::future<std::shared_ptr<GPURenderPipeline>> createRenderPipelineAsync(
      std::shared_ptr<GPURenderPipelineDescriptor> descriptor);
  std::shared_ptr<GPUCommandEncoder> createCommandEncoder(
      std::optional<std::shared_ptr<GPUCommandEncoderDescriptor>> descriptor);
  std::shared_ptr<GPURenderBundleEncoder> createRenderBundleEncoder(
      std::shared_ptr<GPURenderBundleEncoderDescriptor> descriptor);
  std::shared_ptr<GPUQuerySet>
  createQuerySet(std::shared_ptr<GPUQuerySetDescriptor> descriptor);
  void pushErrorScope(wgpu::ErrorFilter filter);
  std::future<std::variant<std::nullptr_t, std::shared_ptr<GPUError>>>
  popErrorScope();

  std::unordered_set<std::string> getFeatures();
  std::shared_ptr<GPUSupportedLimits> getLimits();
  std::shared_ptr<GPUQueue> getQueue();
  std::future<std::shared_ptr<GPUDeviceLostInfo>> getLost();

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDevice::getBrand, this);
    registerHybridMethod("destroy", &GPUDevice::destroy, this);
    registerHybridMethod("createBuffer", &GPUDevice::createBuffer, this);
    registerHybridMethod("createTexture", &GPUDevice::createTexture, this);
    registerHybridMethod("createSampler", &GPUDevice::createSampler, this);
    registerHybridMethod("importExternalTexture",
                         &GPUDevice::importExternalTexture, this);
    registerHybridMethod("createBindGroupLayout",
                         &GPUDevice::createBindGroupLayout, this);
    registerHybridMethod("createPipelineLayout",
                         &GPUDevice::createPipelineLayout, this);
    registerHybridMethod("createBindGroup", &GPUDevice::createBindGroup, this);
    registerHybridMethod("createShaderModule", &GPUDevice::createShaderModule,
                         this);
    registerHybridMethod("createComputePipeline",
                         &GPUDevice::createComputePipeline, this);
    registerHybridMethod("createRenderPipeline",
                         &GPUDevice::createRenderPipeline, this);
    registerHybridMethod("createComputePipelineAsync",
                         &GPUDevice::createComputePipelineAsync, this);
    registerHybridMethod("createRenderPipelineAsync",
                         &GPUDevice::createRenderPipelineAsync, this);
    registerHybridMethod("createCommandEncoder",
                         &GPUDevice::createCommandEncoder, this);
    registerHybridMethod("createRenderBundleEncoder",
                         &GPUDevice::createRenderBundleEncoder, this);
    registerHybridMethod("createQuerySet", &GPUDevice::createQuerySet, this);
    registerHybridMethod("pushErrorScope", &GPUDevice::pushErrorScope, this);
    registerHybridMethod("popErrorScope", &GPUDevice::popErrorScope, this);
    registerHybridGetter("features", &GPUDevice::getFeatures, this);
    registerHybridGetter("limits", &GPUDevice::getLimits, this);
    registerHybridGetter("queue", &GPUDevice::getQueue, this);
    registerHybridGetter("lost", &GPUDevice::getLost, this);
    registerHybridGetter("label", &GPUDevice::getLabel, this);
    registerHybridSetter("label", &GPUDevice::setLabel, this);
   // registerHybridSetter("onuncapturederror", &GPUDevice::setOnuncapturederror, this);
  }

//   void setOnuncapturederror() {
//     ;
//   }

  inline const wgpu::Device get() { return _instance; }

private:
  wgpu::Device _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
  std::shared_ptr<std::promise<std::shared_ptr<GPUDeviceLostInfo>>>
      m_lostPromise;
  std::shared_ptr<std::shared_future<std::shared_ptr<GPUDeviceLostInfo>>>
      m_lostSharedFuture;
};

} // namespace rnwgpu
