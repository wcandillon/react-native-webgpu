#include "GPUDevice.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <ReactCommon/CallInvoker.h>

#include "Convertors.h"
#include "JSIConverter.h"

#include "GPUFeatures.h"
#include "GPUInternalError.h"
#include "GPUOutOfMemoryError.h"
#include "GPUValidationError.h"
#include "RnFeatures.h"

namespace rnwgpu {

namespace {

// Hidden own properties used by device.lost. The promise cache lives on the
// device wrapper (the spec requires `lost` to return a stable promise) and
// the resolve function lives on the promise object itself, so both are traced
// by the GC as part of the device's JS object graph instead of being rooted
// from C++ (issue #445).
constexpr const char *kLostPromiseProp = "__rnwgpuLostPromise";
constexpr const char *kLostResolveProp = "__rnwgpuLostResolve";

void defineHiddenProperty(jsi::Runtime &runtime, const jsi::Object &target,
                          const char *name, const jsi::Value &value) {
  auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
  auto defineProperty =
      objectCtor.getPropertyAsFunction(runtime, "defineProperty");
  jsi::Object descriptor(runtime);
  descriptor.setProperty(runtime, "value", value);
  descriptor.setProperty(runtime, "enumerable", false);
  descriptor.setProperty(runtime, "writable", false);
  descriptor.setProperty(runtime, "configurable", false);
  defineProperty.call(runtime, target,
                      jsi::String::createFromUtf8(runtime, name), descriptor);
}

} // namespace

void GPUDevice::notifyDeviceLost(wgpu::DeviceLostReason reason,
                                 std::string message) {
  std::vector<PendingLostPromise> toResolve;
  std::shared_ptr<GPUDeviceLostInfo> info;
  {
    std::lock_guard<std::mutex> lock(_lostMutex);
    if (_lostSettled) {
      return;
    }

    _lostSettled = true;
    _lostInfo = std::make_shared<GPUDeviceLostInfo>(reason, std::move(message));
    info = _lostInfo;
    toResolve = std::move(_lostPromises);
    _lostPromises.clear();
  }

  if (toResolve.empty()) {
    return;
  }

  // getLost() only registers promises when the device's context has a
  // CallInvoker (main JS runtime), so a non-empty list implies an invoker.
  auto invoker = _async ? _async->callInvoker() : nullptr;
  if (!invoker) {
    return;
  }

  // Settle on the owning runtime's JS thread. The promises are held weakly:
  // if the device graph was collected in the meantime there is nothing to do,
  // since nobody could observe the resolution anyway. The shared_ptr is only
  // there because jsi::WeakObject is move-only and std::function requires a
  // copyable closure; it also ensures the WeakObjects are destroyed on the JS
  // thread.
  auto pending =
      std::make_shared<std::vector<PendingLostPromise>>(std::move(toResolve));
  invoker->invokeAsync([pending, info]() {
    for (auto &entry : *pending) {
      auto &runtime = *entry.runtime;
      auto locked = entry.promise.lock(runtime);
      if (!locked.isObject()) {
        continue;
      }
      auto promiseObj = locked.getObject(runtime);
      auto resolveProp = promiseObj.getProperty(runtime, kLostResolveProp);
      if (!resolveProp.isObject() ||
          !resolveProp.getObject(runtime).isFunction(runtime)) {
        continue;
      }
      auto resolveFn = resolveProp.getObject(runtime).getFunction(runtime);
      resolveFn.call(runtime,
                     JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
                         runtime, info));
    }
    pending->clear();
  });
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
  // The import / begin-access / descriptor-build logic, plus the matching
  // EndAccess, all live on GPUExternalTexture so the begin/end lifecycle stays
  // in one translation unit (see GPUExternalTexture.cpp).
  return GPUExternalTexture::Create(_instance, std::move(descriptor));
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

std::shared_ptr<GPUSharedFence> GPUDevice::importSharedFence(
    std::shared_ptr<GPUSharedFenceDescriptor> descriptor) {
  if (!descriptor || descriptor->handle == nullptr) {
    throw std::runtime_error("GPUDevice::importSharedFence(): handle must be a "
                             "non-null native handle");
  }

  wgpu::SharedFenceDescriptor desc{};
  std::string label = descriptor->label.value_or("");
  if (!label.empty()) {
    desc.label = wgpu::StringView(label.c_str(), label.size());
  }

  // The chained platform descriptor must outlive the synchronous
  // ImportSharedFence() below; declare them all and chain the matching one.
  wgpu::SharedFenceMTLSharedEventDescriptor mtlDesc{};
  wgpu::SharedFenceSyncFDDescriptor syncFdDesc{};
  wgpu::SharedFenceVkSemaphoreOpaqueFDDescriptor vkFdDesc{};

  const std::string &type = descriptor->type;
  if (type == "mtl-shared-event") {
    // handle is an id<MTLSharedEvent> pointer.
    mtlDesc.sharedEvent = descriptor->handle;
    desc.nextInChain = &mtlDesc;
  } else if (type == "sync-fd") {
    // handle is an OS file descriptor.
    syncFdDesc.handle =
        static_cast<int>(reinterpret_cast<uintptr_t>(descriptor->handle));
    desc.nextInChain = &syncFdDesc;
  } else if (type == "vk-semaphore-opaque-fd") {
    vkFdDesc.handle =
        static_cast<int>(reinterpret_cast<uintptr_t>(descriptor->handle));
    desc.nextInChain = &vkFdDesc;
  } else {
    throw std::runtime_error(
        "GPUDevice::importSharedFence(): unsupported fence type '" + type +
        "' (expected 'mtl-shared-event', 'sync-fd' or "
        "'vk-semaphore-opaque-fd')");
  }

  auto fence = _instance.ImportSharedFence(&desc);
  if (fence == nullptr) {
    throw std::runtime_error(
        "GPUDevice::importSharedFence(): ImportSharedFence returned null - is "
        "the matching 'shared-fence-*' feature enabled on the device?");
  }
  return std::make_shared<GPUSharedFence>(std::move(fence), std::move(label));
}

async::AsyncTaskHandle GPUDevice::createComputePipelineAsync(
    jsi::Runtime &runtime,
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

  // Post to the CALLING runtime's context so the promise settles on the
  // thread that requested it (see GPUBuffer::mapAsync).
  auto context =
      async::RuntimeContext::getOrCreate(runtime, _async->instance());
  return context->postTask([device = _instance, desc, descriptor,
                            pipelineHolder](
                               const async::AsyncTaskHandle::ResolveFunction
                                   &resolve,
                               const async::AsyncTaskHandle::RejectFunction
                                   &reject) {
    (void)descriptor;
    device.CreateComputePipelineAsync(
        &desc, wgpu::CallbackMode::AllowProcessEvents,
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
    jsi::Runtime &runtime,
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

  // Post to the CALLING runtime's context so the promise settles on the
  // thread that requested it (see GPUBuffer::mapAsync).
  auto context =
      async::RuntimeContext::getOrCreate(runtime, _async->instance());
  return context->postTask([device = _instance, desc, descriptor,
                            pipelineHolder](
                               const async::AsyncTaskHandle::ResolveFunction
                                   &resolve,
                               const async::AsyncTaskHandle::RejectFunction
                                   &reject) {
    (void)descriptor;
    device.CreateRenderPipelineAsync(
        &desc, wgpu::CallbackMode::AllowProcessEvents,
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

async::AsyncTaskHandle GPUDevice::popErrorScope(jsi::Runtime &runtime) {
  auto device = _instance;

  // Post to the CALLING runtime's context so the promise settles on the
  // thread that requested it (see GPUBuffer::mapAsync).
  auto context =
      async::RuntimeContext::getOrCreate(runtime, _async->instance());
  return context->postTask([device](
                               const async::AsyncTaskHandle::ResolveFunction
                                   &resolve,
                               const async::AsyncTaskHandle::RejectFunction
                                   &reject) {
    device.PopErrorScope(
        wgpu::CallbackMode::AllowProcessEvents,
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
    if (_hidesImplicitSync &&
        feature == wgpu::FeatureName::ImplicitDeviceSynchronization) {
      continue;
    }
    std::string name;
    convertEnumToJSUnion(feature, &name);
    result.insert(name);
  }
  maybeSynthesizeRnNativeTextureFeature(enabled, result);
  return result;
}

jsi::Value GPUDevice::getLost(jsi::Runtime &runtime,
                              const jsi::Object &wrapper) {
  // The promise is cached on the wrapper, not natively: a strong native
  // reference would be a GC root, keeping the promise's .then reactions (and
  // anything they capture, e.g. a whole three.js renderer) alive forever
  // (issue #445).
  auto cached = wrapper.getProperty(runtime, kLostPromiseProp);
  if (cached.isObject()) {
    return cached;
  }

  auto promiseCtor = runtime.global().getPropertyAsObject(runtime, "Promise");

  std::shared_ptr<GPUDeviceLostInfo> settledInfo;
  {
    std::lock_guard<std::mutex> lock(_lostMutex);
    if (_lostSettled) {
      settledInfo = _lostInfo;
    }
  }

  jsi::Value promiseValue;
  if (settledInfo) {
    auto info = JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
        runtime, settledInfo);
    promiseValue = promiseCtor.getPropertyAsFunction(runtime, "resolve")
                       .callWithThis(runtime, promiseCtor, info);
  } else {
    // new Promise(executor): the executor runs synchronously inside the
    // constructor call, capturing the resolve function.
    auto capturedResolve = std::make_shared<jsi::Value>();
    auto executor = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "lostExecutor"), 2,
        [capturedResolve](jsi::Runtime &rt, const jsi::Value & /*thisVal*/,
                          const jsi::Value *args, size_t count) -> jsi::Value {
          if (count > 0) {
            *capturedResolve = jsi::Value(rt, args[0]);
          }
          return jsi::Value::undefined();
        });
    auto promiseObj = promiseCtor.asFunction(runtime)
                          .callAsConstructor(runtime, executor)
                          .getObject(runtime);

    // Stash the resolve function on the promise itself so the GC traces it as
    // part of the promise graph.
    defineHiddenProperty(runtime, promiseObj, kLostResolveProp,
                         *capturedResolve);

    // Register a WEAK reference so notifyDeviceLost() can settle the promise
    // if it is still alive. Only wired for the device's own runtime when a
    // CallInvoker exists (main JS runtime): spontaneous events are delivered
    // through that invoker. A promise created on another runtime stays
    // pending, matching the previous best-effort behavior.
    bool settledMeanwhile = false;
    if (_async && _async->callInvoker() && &runtime == &_async->runtime()) {
      std::lock_guard<std::mutex> lock(_lostMutex);
      if (_lostSettled) {
        // The device was lost between the check above and now.
        settledMeanwhile = true;
        settledInfo = _lostInfo;
      } else {
        _lostPromises.push_back(
            PendingLostPromise{&runtime, jsi::WeakObject(runtime, promiseObj)});
      }
    }
    if (settledMeanwhile) {
      auto resolveFn =
          promiseObj.getPropertyAsFunction(runtime, kLostResolveProp);
      resolveFn.call(runtime,
                     JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(
                         runtime, settledInfo));
    }
    promiseValue = jsi::Value(runtime, promiseObj);
  }

  defineHiddenProperty(runtime, wrapper, kLostPromiseProp, promiseValue);
  return promiseValue;
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
  // Dawn can surface an uncaptured error from any ProcessEvents pump (a worklet
  // runtime sharing this instance may pump it on the wrong thread). Marshal to
  // the owning runtime's JS thread via its CallInvoker before touching JSI. The
  // invoker is wired only for the main JS runtime, so a device created on a
  // worklet runtime does not deliver uncaptured errors to JS (best-effort; see
  // README "Threading model").
  auto invoker = _async ? _async->callInvoker() : nullptr;
  if (!invoker) {
    return;
  }
  auto self = shared_from_this();
  invoker->invokeAsync([self, type, message = std::move(message)]() mutable {
    self->deliverUncapturedError(type, std::move(message));
  });
}

void GPUDevice::deliverUncapturedError(wgpu::ErrorType type,
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
