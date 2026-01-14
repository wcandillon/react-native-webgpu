#include "RNWebGPUManager.h"

#include "GPU.h"
#include "NativeObject.h"
#include "RNWebGPU.h"

// GPU API classes (for instanceof support)
#include "GPUAdapter.h"
#include "GPUAdapterInfo.h"
#include "GPUBindGroup.h"
#include "GPUBindGroupLayout.h"
#include "GPUBuffer.h"
#include "GPUCanvasContext.h"
#include "GPUCommandBuffer.h"
#include "GPUCommandEncoder.h"
#include "GPUCompilationInfo.h"
#include "GPUCompilationMessage.h"
#include "GPUComputePassEncoder.h"
#include "GPUComputePipeline.h"
#include "GPUDevice.h"
#include "GPUDeviceLostInfo.h"
#include "GPUExternalTexture.h"
#include "GPUPipelineLayout.h"
#include "GPUQuerySet.h"
#include "GPUQueue.h"
#include "GPURenderBundle.h"
#include "GPURenderBundleEncoder.h"
#include "GPURenderPassEncoder.h"
#include "GPURenderPipeline.h"
#include "GPUSampler.h"
#include "GPUShaderModule.h"
#include "GPUSupportedLimits.h"
#include "GPUTexture.h"
#include "GPUTextureView.h"

// Enums
#include "GPUBufferUsage.h"
#include "GPUColorWrite.h"
#include "GPUMapMode.h"
#include "GPUShaderStage.h"
#include "GPUTextureUsage.h"

#include <memory>
#include <utility>

namespace rnwgpu {

RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
    std::shared_ptr<PlatformContext> platformContext)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker),
      _platformContext(platformContext) {

  // Register main runtime for RuntimeAwareCache
  BaseRuntimeAwareCache::setMainJsRuntime(_jsRuntime);

  auto gpu = std::make_shared<GPU>(*_jsRuntime);
  auto rnWebGPU = std::make_shared<RNWebGPU>(gpu, _platformContext);
  _gpu = gpu->get();
  _jsRuntime->global().setProperty(*_jsRuntime, "RNWebGPU",
                                   RNWebGPU::create(*_jsRuntime, rnWebGPU));

  // Install constructors for instanceof support
  GPU::installConstructor(*_jsRuntime);
  GPUAdapter::installConstructor(*_jsRuntime);
  GPUAdapterInfo::installConstructor(*_jsRuntime);
  GPUBindGroup::installConstructor(*_jsRuntime);
  GPUBindGroupLayout::installConstructor(*_jsRuntime);
  GPUBuffer::installConstructor(*_jsRuntime);
  GPUCanvasContext::installConstructor(*_jsRuntime);
  GPUCommandBuffer::installConstructor(*_jsRuntime);
  GPUCommandEncoder::installConstructor(*_jsRuntime);
  GPUCompilationInfo::installConstructor(*_jsRuntime);
  GPUCompilationMessage::installConstructor(*_jsRuntime);
  GPUComputePassEncoder::installConstructor(*_jsRuntime);
  GPUComputePipeline::installConstructor(*_jsRuntime);
  GPUDevice::installConstructor(*_jsRuntime);
  GPUDeviceLostInfo::installConstructor(*_jsRuntime);
  GPUExternalTexture::installConstructor(*_jsRuntime);
  GPUPipelineLayout::installConstructor(*_jsRuntime);
  GPUQuerySet::installConstructor(*_jsRuntime);
  GPUQueue::installConstructor(*_jsRuntime);
  GPURenderBundle::installConstructor(*_jsRuntime);
  GPURenderBundleEncoder::installConstructor(*_jsRuntime);
  GPURenderPassEncoder::installConstructor(*_jsRuntime);
  GPURenderPipeline::installConstructor(*_jsRuntime);
  GPUSampler::installConstructor(*_jsRuntime);
  GPUShaderModule::installConstructor(*_jsRuntime);
  GPUSupportedLimits::installConstructor(*_jsRuntime);
  GPUTexture::installConstructor(*_jsRuntime);
  GPUTextureView::installConstructor(*_jsRuntime);

  // Install constant objects as plain JS objects with own properties
  _jsRuntime->global().setProperty(*_jsRuntime, "GPUBufferUsage",
                                   GPUBufferUsage::create(*_jsRuntime));
  _jsRuntime->global().setProperty(*_jsRuntime, "GPUColorWrite",
                                   GPUColorWrite::create(*_jsRuntime));
  _jsRuntime->global().setProperty(*_jsRuntime, "GPUMapMode",
                                   GPUMapMode::create(*_jsRuntime));
  _jsRuntime->global().setProperty(*_jsRuntime, "GPUShaderStage",
                                   GPUShaderStage::create(*_jsRuntime));
  _jsRuntime->global().setProperty(*_jsRuntime, "GPUTextureUsage",
                                   GPUTextureUsage::create(*_jsRuntime));
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}

} // namespace rnwgpu
