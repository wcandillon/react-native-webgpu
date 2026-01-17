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
#include "GPUSupportedLimits.h"
#include "GPUTexture.h"
#include "GPUTextureView.h"
#include "GPUValidationError.h"

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
  GPUError::installConstructor(*_jsRuntime);
  GPUExternalTexture::installConstructor(*_jsRuntime);
  GPUInternalError::installConstructor(*_jsRuntime);
  GPUOutOfMemoryError::installConstructor(*_jsRuntime);
  GPUValidationError::installConstructor(*_jsRuntime);
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

  // Install global helper functions for Worklets serialization
  // These are standalone functions that don't require RNWebGPU instance
  installWebGPUWorkletHelpers(*_jsRuntime);
}

void RNWebGPUManager::installWebGPUWorkletHelpers(jsi::Runtime &runtime) {
  // __webgpuIsWebGPUObject - checks if a value is a WebGPU NativeObject
  auto isWebGPUObjectFunc = jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forUtf8(runtime, "__webgpuIsWebGPUObject"), 1,
      [](jsi::Runtime &rt, const jsi::Value & /*thisVal*/,
         const jsi::Value *args, size_t count) -> jsi::Value {
        if (count < 1 || !args[0].isObject()) {
          return jsi::Value(false);
        }
        auto obj = args[0].getObject(rt);

        // Check if it has native state
        if (!obj.hasNativeState(rt)) {
          return jsi::Value(false);
        }

        // Check if it has Symbol.toStringTag on its prototype (WebGPU objects
        // do)
        auto objectCtor = rt.global().getPropertyAsObject(rt, "Object");
        auto getPrototypeOf =
            objectCtor.getPropertyAsFunction(rt, "getPrototypeOf");
        auto proto = getPrototypeOf.call(rt, obj);

        if (!proto.isObject()) {
          return jsi::Value(false);
        }

        auto protoObj = proto.getObject(rt);
        auto symbolCtor = rt.global().getPropertyAsObject(rt, "Symbol");
        auto toStringTag = symbolCtor.getProperty(rt, "toStringTag");
        if (toStringTag.isUndefined()) {
          return jsi::Value(false);
        }

        auto getOwnPropertyDescriptor =
            objectCtor.getPropertyAsFunction(rt, "getOwnPropertyDescriptor");
        auto desc = getOwnPropertyDescriptor.call(rt, protoObj, toStringTag);
        return jsi::Value(desc.isObject());
      });
  runtime.global().setProperty(runtime, "__webgpuIsWebGPUObject",
                               std::move(isWebGPUObjectFunc));

  // __webgpuBox - boxes a WebGPU object for Worklets serialization
  auto boxFunc = jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forUtf8(runtime, "__webgpuBox"), 1,
      [](jsi::Runtime &rt, const jsi::Value & /*thisVal*/,
         const jsi::Value *args, size_t count) -> jsi::Value {
        if (count < 1 || !args[0].isObject()) {
          throw jsi::JSError(rt,
                             "__webgpuBox() requires a WebGPU object argument");
        }

        auto obj = args[0].getObject(rt);

        // Check if it has native state
        if (!obj.hasNativeState(rt)) {
          throw jsi::JSError(
              rt, "Object has no native state - not a WebGPU object");
        }

        // Get the brand name from Symbol.toStringTag on the prototype
        auto objectCtor = rt.global().getPropertyAsObject(rt, "Object");
        auto getPrototypeOf =
            objectCtor.getPropertyAsFunction(rt, "getPrototypeOf");
        auto proto = getPrototypeOf.call(rt, obj);

        std::string brand;
        if (proto.isObject()) {
          auto protoObj = proto.getObject(rt);
          auto symbolCtor = rt.global().getPropertyAsObject(rt, "Symbol");
          auto toStringTag = symbolCtor.getProperty(rt, "toStringTag");
          if (!toStringTag.isUndefined()) {
            auto getOwnPropertyDescriptor = objectCtor.getPropertyAsFunction(
                rt, "getOwnPropertyDescriptor");
            auto desc =
                getOwnPropertyDescriptor.call(rt, protoObj, toStringTag);
            if (desc.isObject()) {
              auto descObj = desc.getObject(rt);
              auto value = descObj.getProperty(rt, "value");
              if (value.isString()) {
                brand = value.getString(rt).utf8(rt);
              }
            }
          }
        }

        if (brand.empty()) {
          throw jsi::JSError(rt, "Cannot determine WebGPU object type - no "
                                 "Symbol.toStringTag found");
        }

        auto nativeState = obj.getNativeState(rt);
        auto boxed = std::make_shared<BoxedWebGPUObject>(nativeState, brand);
        return jsi::Object::createFromHostObject(rt, boxed);
      });
  runtime.global().setProperty(runtime, "__webgpuBox", std::move(boxFunc));
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}

} // namespace rnwgpu
