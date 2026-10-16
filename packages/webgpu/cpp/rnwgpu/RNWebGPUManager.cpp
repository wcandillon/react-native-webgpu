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
#include "GPUError.h"
#include "GPUExternalTexture.h"
#include "GPUInternalError.h"
#include "GPUNativeBufferExternalTexture.h"
#include "GPUNativeBufferTexture.h"
#include "GPUOutOfMemoryError.h"
#include "GPUPipelineLayout.h"
#include "GPUQuerySet.h"
#include "GPUQueue.h"
#include "GPURenderBundle.h"
#include "GPURenderBundleEncoder.h"
#include "GPURenderPassEncoder.h"
#include "GPURenderPipeline.h"
#include "GPUSampler.h"
#include "GPUShaderModule.h"
#include "GPUSharedFence.h"
#include "GPUSharedTextureMemory.h"
#include "GPUSupportedLimits.h"
#include "GPUTexture.h"
#include "GPUTextureView.h"
#include "GPUUncapturedErrorEvent.h"
#include "GPUValidationError.h"
#include "VideoFrame.h"
#include "VideoPlayer.h"

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

  // Register the main runtime + its CallInvoker so spontaneous events
  // (device.lost / uncapturederror) on main-runtime devices can be delivered to
  // the JS thread without the ProcessEvents pump. Worklet-runtime devices have
  // no invoker (best-effort; see README "Threading model").
  async::RuntimeContext::registerMainRuntime(_jsRuntime, _jsCallInvoker);

  // Expose the platform context to API objects created later without a
  // reference to it (GPUQueue::drawElementImageToTexture).
  PlatformContext::setCurrent(_platformContext);

  auto gpu = std::make_shared<GPU>(*_jsRuntime);
  auto rnWebGPU =
      std::make_shared<RNWebGPU>(gpu, _platformContext, _jsCallInvoker);
  _gpu = gpu->get();

  // RNWebGPU needs its brand registered in NativeObjectRegistry so the boxing
  // path can install the prototype on worklet runtimes. installConstructor
  // does that registration but also sets globalThis.RNWebGPU = ctor, so we
  // call it FIRST and then overwrite the global with the actual instance.
  RNWebGPU::installConstructor(*_jsRuntime);
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
  GPUError::installConstructor(*_jsRuntime);
  GPUExternalTexture::installConstructor(*_jsRuntime);
  GPUNativeBufferExternalTexture::installConstructor(*_jsRuntime);
  GPUNativeBufferTexture::installConstructor(*_jsRuntime);
  GPUInternalError::installConstructor(*_jsRuntime);
  GPUOutOfMemoryError::installConstructor(*_jsRuntime);
  GPUValidationError::installConstructor(*_jsRuntime);
  GPUUncapturedErrorEvent::installConstructor(*_jsRuntime);
  GPUPipelineLayout::installConstructor(*_jsRuntime);
  GPUQuerySet::installConstructor(*_jsRuntime);
  GPUQueue::installConstructor(*_jsRuntime);
  GPURenderBundle::installConstructor(*_jsRuntime);
  GPURenderBundleEncoder::installConstructor(*_jsRuntime);
  GPURenderPassEncoder::installConstructor(*_jsRuntime);
  GPURenderPipeline::installConstructor(*_jsRuntime);
  GPUSampler::installConstructor(*_jsRuntime);
  GPUSharedFence::installConstructor(*_jsRuntime);
  GPUSharedTextureMemory::installConstructor(*_jsRuntime);
  GPUShaderModule::installConstructor(*_jsRuntime);
  GPUSupportedLimits::installConstructor(*_jsRuntime);
  GPUTexture::installConstructor(*_jsRuntime);
  GPUTextureView::installConstructor(*_jsRuntime);
  VideoFrame::installConstructor(*_jsRuntime);
  VideoPlayer::installConstructor(*_jsRuntime);

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

  // Install global helper functions for Worklets serialization
  // These are standalone functions that don't require RNWebGPU instance
  installWebGPUWorkletHelpers(*_jsRuntime);
}

void RNWebGPUManager::installWebGPUWorkletHelpers(jsi::Runtime &runtime) {
  // The serializer worklets capture this object by closure; worklets passes a
  // HostObject to every runtime by reference, so installing it here, on the
  // main runtime, is enough (see WebGPUWorkletHelpers).
  runtime.global().setProperty(
      runtime, "__webgpuWorkletHelpers",
      jsi::Object::createFromHostObject(
          runtime, std::make_shared<WebGPUWorkletHelpers>()));
}

void RNWebGPUManager::flushPendingSurfaceTransition(
    std::shared_ptr<SurfaceInfo> info) {
  if (info == nullptr || _jsCallInvoker == nullptr) {
    return;
  }
  _jsCallInvoker->invokeAsync(
      [info = std::move(info)] { info->applyPendingAttach(); });
}

RNWebGPUManager::~RNWebGPUManager() {
  // Drop all canvas registry entries: after a reload the JS side restarts its
  // contextId counter, and stale entries would alias new canvases onto dead
  // surfaces.
  SurfaceRegistry::getInstance().clear();
  if (PlatformContext::current() == _platformContext) {
    PlatformContext::setCurrent(nullptr);
  }
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}

} // namespace rnwgpu
