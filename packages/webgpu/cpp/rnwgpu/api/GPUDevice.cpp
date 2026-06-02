#include "GPUDevice.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Convertors.h"
#include "JSIConverter.h"

#include "GPUFeatures.h"
#include "GPUInternalError.h"
#include "GPUOutOfMemoryError.h"
#include "GPUValidationError.h"
#include "RnFeatures.h"

namespace rnwgpu {

void GPUDevice::notifyDeviceLost(wgpu::DeviceLostReason reason,
                                 std::string message) {
  std::optional<async::AsyncTaskHandle::ResolveFunction> resolveToCall;
  std::shared_ptr<GPUDeviceLostInfo> info;
  {
    std::lock_guard<std::mutex> lock(_lostMutex);
    if (_lostSettled) {
      return;
    }

    _lostSettled = true;
    _lostInfo = std::make_shared<GPUDeviceLostInfo>(reason, std::move(message));
    info = _lostInfo;

    if (_lostResolve.has_value()) {
      resolveToCall = std::move(*_lostResolve);
      _lostResolve.reset();
    }

    _lostHandle.reset();
  }

  // Settle outside the lock: resolve() only enqueues onto the JS thread.
  if (resolveToCall.has_value()) {
    (*resolveToCall)([info](jsi::Runtime &runtime) mutable {
      return JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(runtime,
                                                                     info);
    });
  }
}

void GPUDevice::forceLossForTesting() {
  // wgpu::StringView view("forceLossForTesting invoked from JS");
  _instance.ForceLoss(wgpu::DeviceLostReason::Unknown,
                      "forceLossForTesting invoked from JS");
}

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

std::shared_ptr<GPUSupportedLimits> GPUDevice::getLimits() {
  wgpu::Limits limits{};
  if (!_instance.GetLimits(&limits)) {
    throw std::runtime_error("failed to get device limits");
  }
  return std::make_shared<GPUSupportedLimits>(limits);
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

void GPUDevice::destroy() {
  _instance.Destroy();
  notifyDeviceLost(wgpu::DeviceLostReason::Destroyed, "device was destroyed");
}

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  wgpu::TextureDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUTextureDescriptor");
  }
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture, descriptor->label.value_or(""));
}

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderSourceWGSL wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  Convertor conv;
  if (!conv(wgsl_desc.code, descriptor->code) ||
      !conv(sm_desc.label, descriptor->label)) {
    return {};
  }
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    auto mod = _instance.CreateErrorShaderModule(
        &sm_desc, "The WGSL shader contains an illegal character '\\0'");
    return std::make_shared<GPUShaderModule>(mod, _async, sm_desc.label.data);
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
  // assert(desc.fragment != nullptr && "Fragment state must not be null");
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

std::shared_ptr<GPUBindGroupLayout> GPUDevice::createBindGroupLayout(
    std::shared_ptr<GPUBindGroupLayoutDescriptor> descriptor) {
  Convertor conv;

  wgpu::BindGroupLayoutDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.entries, desc.entryCount, descriptor->entries)) {
    return {};
  }
  return std::make_shared<GPUBindGroupLayout>(
      _instance.CreateBindGroupLayout(&desc), descriptor->label.value_or(""));
}

std::shared_ptr<GPUPipelineLayout> GPUDevice::createPipelineLayout(
    std::shared_ptr<GPUPipelineLayoutDescriptor> descriptor) {
  Convertor conv;

  wgpu::PipelineLayoutDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.bindGroupLayouts, desc.bindGroupLayoutCount,
            descriptor->bindGroupLayouts)) {
    return {};
  }
  return std::make_shared<GPUPipelineLayout>(
      _instance.CreatePipelineLayout(&desc), descriptor->label.value_or(""));
}

std::shared_ptr<GPUExternalTexture> GPUDevice::importExternalTexture(
    std::shared_ptr<GPUExternalTextureDescriptor> descriptor) {
  throw std::runtime_error(
      "GPUDevice::importExternalTexture(): Not implemented");
}

std::shared_ptr<GPUSharedTextureMemory> GPUDevice::importSharedTextureMemory(
    std::shared_ptr<GPUSharedTextureMemoryDescriptor> descriptor) {
  if (!descriptor || descriptor->handle == nullptr) {
    throw std::runtime_error("GPUDevice::importSharedTextureMemory(): handle "
                             "must be a non-null native pointer");
  }

  wgpu::SharedTextureMemoryDescriptor desc{};
  std::string label = descriptor->label.value_or("");
  if (!label.empty()) {
    desc.label = wgpu::StringView(label.c_str(), label.size());
  }

#if defined(__APPLE__)
  wgpu::SharedTextureMemoryIOSurfaceDescriptor platformDesc{};
  platformDesc.ioSurface = descriptor->handle;
  // Default off: enabling it propagates StorageBinding into properties.usage,
  // which then forces memory.createTexture() (no-descriptor form) to validate
  // the format against storage capabilities. bgra8unorm (the standard
  // CVPixelBuffer format) only supports storage when the device opts into the
  // bgra8unorm-storage feature, so unconditionally setting this here breaks
  // the common sample-only case.
  platformDesc.allowStorageBinding = false;
  desc.nextInChain = &platformDesc;
#elif defined(__ANDROID__)
  wgpu::SharedTextureMemoryAHardwareBufferDescriptor platformDesc{};
  platformDesc.handle = descriptor->handle;
  desc.nextInChain = &platformDesc;
#else
  throw std::runtime_error(
      "GPUDevice::importSharedTextureMemory(): unsupported platform");
#endif

  auto memory = _instance.ImportSharedTextureMemory(&desc);
  if (memory == nullptr) {
    throw std::runtime_error("GPUDevice::importSharedTextureMemory(): "
                             "ImportSharedTextureMemory returned null - is the "
                             "'shared-texture-memory-iosurface' (Apple) or "
                             "'shared-texture-memory-ahardware-buffer' "
                             "(Android) feature enabled on the device?");
  }
  return std::make_shared<GPUSharedTextureMemory>(std::move(memory),
                                                  std::move(label));
}

async::AsyncTaskHandle GPUDevice::createComputePipelineAsync(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                             "GPUComputePipelineDescriptor");
  }

  auto label = std::string(
      descriptor->label.has_value() ? descriptor->label.value() : "");
  auto pipelineHolder = std::make_shared<GPUComputePipeline>(nullptr, label);

  return _async->postTask([device = _instance, desc, descriptor,
                           pipelineHolder](
                              const async::AsyncTaskHandle::ResolveFunction
                                  &resolve,
                              const async::AsyncTaskHandle::RejectFunction
                                  &reject) -> wgpu::Future {
    (void)descriptor;
    return device.CreateComputePipelineAsync(
        &desc, wgpu::CallbackMode::WaitAnyOnly,
        [pipelineHolder, resolve,
         reject](wgpu::CreatePipelineAsyncStatus status,
                 wgpu::ComputePipeline pipeline, wgpu::StringView msg) {
          if (status == wgpu::CreatePipelineAsyncStatus::Success && pipeline) {
            pipelineHolder->_instance = pipeline;
            resolve([pipelineHolder](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUComputePipeline>>::toJSI(
                  runtime, pipelineHolder);
            });
          } else {
            std::string error = msg.length
                                    ? std::string(msg.data, msg.length)
                                    : "Failed to create compute pipeline";
            reject(std::move(error));
          }
        });
  });
}

async::AsyncTaskHandle GPUDevice::createRenderPipelineAsync(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  wgpu::RenderPipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error(
        "GPUDevice::createRenderPipelineAsync(): Error with "
        "GPURenderPipelineDescriptor");
  }

  auto label = std::string(
      descriptor->label.has_value() ? descriptor->label.value() : "");
  auto pipelineHolder = std::make_shared<GPURenderPipeline>(nullptr, label);

  return _async->postTask([device = _instance, desc, descriptor,
                           pipelineHolder](
                              const async::AsyncTaskHandle::ResolveFunction
                                  &resolve,
                              const async::AsyncTaskHandle::RejectFunction
                                  &reject) -> wgpu::Future {
    (void)descriptor;
    return device.CreateRenderPipelineAsync(
        &desc, wgpu::CallbackMode::WaitAnyOnly,
        [pipelineHolder, resolve,
         reject](wgpu::CreatePipelineAsyncStatus status,
                 wgpu::RenderPipeline pipeline, wgpu::StringView msg) {
          if (status == wgpu::CreatePipelineAsyncStatus::Success && pipeline) {
            pipelineHolder->_instance = pipeline;
            resolve([pipelineHolder](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPURenderPipeline>>::toJSI(
                  runtime, pipelineHolder);
            });
          } else {
            std::string error = msg.length ? std::string(msg.data, msg.length)
                                           : "Failed to create render pipeline";
            reject(std::move(error));
          }
        });
  });
}

void GPUDevice::pushErrorScope(wgpu::ErrorFilter filter) {
  _instance.PushErrorScope(filter);
}

async::AsyncTaskHandle GPUDevice::popErrorScope() {
  auto device = _instance;

  return _async->postTask([device](const async::AsyncTaskHandle::ResolveFunction
                                       &resolve,
                                   const async::AsyncTaskHandle::RejectFunction
                                       &reject) -> wgpu::Future {
    return device.PopErrorScope(
        wgpu::CallbackMode::WaitAnyOnly,
        [resolve, reject](wgpu::PopErrorScopeStatus status,
                          wgpu::ErrorType type, wgpu::StringView message) {
          if (status == wgpu::PopErrorScopeStatus::Error ||
              status == wgpu::PopErrorScopeStatus::CallbackCancelled) {
            reject("PopErrorScope failed");
            return;
          }

          std::string messageString =
              message.length ? std::string(message.data, message.length) : "";

          switch (type) {
          case wgpu::ErrorType::NoError:
            resolve([](jsi::Runtime &runtime) mutable {
              return jsi::Value::null();
            });
            break;
          case wgpu::ErrorType::Validation: {
            auto error = std::make_shared<GPUValidationError>(messageString);
            resolve([error](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUValidationError>>::toJSI(
                  runtime, error);
            });
            break;
          }
          case wgpu::ErrorType::OutOfMemory: {
            auto error = std::make_shared<GPUOutOfMemoryError>(messageString);
            resolve([error](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUOutOfMemoryError>>::toJSI(
                  runtime, error);
            });
            break;
          }
          case wgpu::ErrorType::Internal:
          case wgpu::ErrorType::Unknown: {
            auto error = std::make_shared<GPUInternalError>(messageString);
            resolve([error](jsi::Runtime &runtime) mutable {
              return JSIConverter<std::shared_ptr<GPUInternalError>>::toJSI(
                  runtime, error);
            });
            break;
          }
          default:
            reject("Unhandled GPU error type");
            return;
          }
        });
  });
}

std::unordered_set<std::string> GPUDevice::getFeatures() {
  wgpu::SupportedFeatures supportedFeatures;
  _instance.GetFeatures(&supportedFeatures);
  std::unordered_set<std::string> result;
  std::unordered_set<wgpu::FeatureName> enabled;
  for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
    auto feature = supportedFeatures.features[i];
    enabled.insert(feature);
    std::string name;
    convertEnumToJSUnion(feature, &name);
    result.insert(name);
  }
  maybeSynthesizeRnSharedTextureMemoryFeature(enabled, result);
  return result;
}

async::AsyncTaskHandle GPUDevice::getLost() {
  // Held across the whole body: the postTask callback below runs synchronously
  // on this (JS) thread and touches the same _lost* fields, so it must not
  // re-lock. notifyDeviceLost() takes the same lock from its (possibly worker)
  // thread.
  std::lock_guard<std::mutex> lock(_lostMutex);
  if (_lostHandle.has_value()) {
    return *_lostHandle;
  }

  if (_lostSettled && _lostInfo) {
    return _async->postTask(
        [info = _lostInfo](
            const async::AsyncTaskHandle::ResolveFunction &resolve,
            const async::AsyncTaskHandle::RejectFunction & /*reject*/)
            -> wgpu::Future {
          resolve([info](jsi::Runtime &runtime) mutable {
            return JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
                runtime, info);
          });
          // No Dawn event to wait on: resolved synchronously.
          return wgpu::Future{};
        });
  }

  auto handle = _async->postTask(
      [this](const async::AsyncTaskHandle::ResolveFunction &resolve,
             const async::AsyncTaskHandle::RejectFunction & /*reject*/)
          -> wgpu::Future {
        if (_lostSettled && _lostInfo) {
          resolve([info = _lostInfo](jsi::Runtime &runtime) mutable {
            return JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
                runtime, info);
          });
          return wgpu::Future{};
        }

        // Resolved later from notifyDeviceLost(); no Dawn event to wait on.
        _lostResolve = resolve;
        return wgpu::Future{};
      });

  _lostHandle = handle;
  return handle;
}
void GPUDevice::addEventListener(std::string type, jsi::Function callback) {
  auto funcPtr = std::make_shared<jsi::Function>(std::move(callback));
  _eventListeners[type].push_back(funcPtr);
}

void GPUDevice::removeEventListener(std::string type, jsi::Function callback) {
  // Note: Since jsi::Function doesn't support equality comparison,
  // we cannot reliably remove a specific listener. This is a no-op.
  // Most use cases (like BabylonJS) only need addEventListener to work.
  (void)type;
  (void)callback;
}

void GPUDevice::notifyUncapturedError(wgpu::ErrorType type,
                                      std::string message) {
  auto it = _eventListeners.find("uncapturederror");
  if (it == _eventListeners.end() || it->second.empty()) {
    return;
  }

  auto runtime = getCreationRuntime();
  if (runtime == nullptr) {
    return;
  }

  // Create the appropriate error object based on type
  GPUErrorVariant error;
  switch (type) {
  case wgpu::ErrorType::Validation:
    error = std::make_shared<GPUValidationError>(message);
    break;
  case wgpu::ErrorType::OutOfMemory:
    error = std::make_shared<GPUOutOfMemoryError>(message);
    break;
  case wgpu::ErrorType::Internal:
  case wgpu::ErrorType::Unknown:
  default:
    error = std::make_shared<GPUInternalError>(message);
    break;
  }

  // Create the event object
  auto event = std::make_shared<GPUUncapturedErrorEvent>(std::move(error));
  auto eventValue =
      JSIConverter<std::shared_ptr<GPUUncapturedErrorEvent>>::toJSI(*runtime,
                                                                    event);

  // Call all registered listeners
  for (const auto &listener : it->second) {
    try {
      listener->call(*runtime, eventValue);
    } catch (const std::exception &e) {
      // Log but don't throw - we don't want one listener to break others
      fprintf(stderr, "Error in uncapturederror listener: %s\n", e.what());
    }
  }
}

} // namespace rnwgpu
