#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Unions.h"

#include "NativeObject.h"

#include "rnwgpu/async/AsyncTaskHandle.h"
#include "rnwgpu/async/RuntimeContext.h"

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
#include "GPUSharedFenceDescriptor.h"
#include "GPUSharedTextureMemory.h"
#include "GPUSharedTextureMemoryDescriptor.h"
#include "GPUSupportedLimits.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "GPUUncapturedErrorEvent.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUDevice : public NativeObject<GPUDevice> {
private:
  struct RuntimeEventListeners final {
    void clear() {
      std::lock_guard<std::mutex> lock(mutex);
      listeners.clear();
    }

    std::mutex mutex;
    std::unordered_map<std::string, std::vector<std::shared_ptr<jsi::Function>>>
        listeners;
  };

  struct DeviceRegistry final {
    std::mutex mutex;
    std::unordered_map<WGPUDevice, std::weak_ptr<GPUDevice>> devices;
  };

  static DeviceRegistry &getRegistry() {
    // Dawn callbacks can arrive during process-wide static teardown.
    static auto *registry = new DeviceRegistry();
    return *registry;
  }

public:
  static constexpr const char *CLASS_NAME = "GPUDevice";

  explicit GPUDevice(wgpu::Device instance,
                     std::shared_ptr<async::RuntimeContext> async,
                     std::string label)
      : NativeObject(CLASS_NAME), _instance(instance), _async(async),
        _label(std::move(label)),
        _eventListeners(std::make_shared<RuntimeEventListeners>()) {}

  ~GPUDevice() override {
    // Unregister from the static registry
    unregisterDevice(_instance.Get(), this);
    // The callback retains listener state until the owning runtime thread has
    // cleared its JSI functions (or runtime invalidation performs the cleanup).
    if (_async && _listenerCleanupCallbackId != 0) {
      _async->removeInvalidationCallback(_listenerCleanupCallbackId);
    }
  }

  // Static registry for looking up GPUDevice from wgpu::Device in callbacks
  static void registerDevice(WGPUDevice handle,
                             std::weak_ptr<GPUDevice> device) {
    auto &registry = getRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.devices.insert_or_assign(handle, std::move(device));
  }

  static void unregisterDevice(WGPUDevice handle, const GPUDevice *expected) {
    auto &registry = getRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto it = registry.devices.find(handle);
    if (it == registry.devices.end()) {
      return;
    }
    const auto current = it->second.lock();
    if (!current || current.get() == expected) {
      registry.devices.erase(it);
    }
  }

  static std::shared_ptr<GPUDevice> lookupDevice(WGPUDevice handle) {
    auto &registry = getRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto it = registry.devices.find(handle);
    if (it != registry.devices.end()) {
      return it->second.lock();
    }
    return nullptr;
  }

public:
  std::string getBrand() { return CLASS_NAME; }

  void destroy();
  std::shared_ptr<GPUBuffer>
  createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor);
  std::shared_ptr<GPUTexture>
  createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor);
  std::shared_ptr<GPUSampler> createSampler(
      std::optional<std::shared_ptr<GPUSamplerDescriptor>> descriptor);
  std::shared_ptr<GPUExternalTexture> importExternalTexture(
      std::shared_ptr<GPUExternalTextureDescriptor> descriptor);
  std::shared_ptr<GPUSharedTextureMemory> importSharedTextureMemory(
      std::shared_ptr<GPUSharedTextureMemoryDescriptor> descriptor);
  std::shared_ptr<GPUSharedFence>
  importSharedFence(std::shared_ptr<GPUSharedFenceDescriptor> descriptor);
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
  async::AsyncTaskHandle createComputePipelineAsync(
      jsi::Runtime &runtime,
      std::shared_ptr<GPUComputePipelineDescriptor> descriptor);
  async::AsyncTaskHandle createRenderPipelineAsync(
      jsi::Runtime &runtime,
      std::shared_ptr<GPURenderPipelineDescriptor> descriptor);
  std::shared_ptr<GPUCommandEncoder> createCommandEncoder(
      std::optional<std::shared_ptr<GPUCommandEncoderDescriptor>> descriptor);
  std::shared_ptr<GPURenderBundleEncoder> createRenderBundleEncoder(
      std::shared_ptr<GPURenderBundleEncoderDescriptor> descriptor);
  std::shared_ptr<GPUQuerySet>
  createQuerySet(std::shared_ptr<GPUQuerySetDescriptor> descriptor);
  void pushErrorScope(wgpu::ErrorFilter filter);
  async::AsyncTaskHandle popErrorScope(jsi::Runtime &runtime);

  std::unordered_set<std::string> getFeatures();
  std::shared_ptr<GPUSupportedLimits> getLimits();
  std::shared_ptr<GPUQueue> getQueue();
  async::AsyncTaskHandle getLost(jsi::Runtime &runtime);
  void notifyDeviceLost(wgpu::DeviceLostReason reason, std::string message);
  void notifyUncapturedError(wgpu::ErrorType type, std::string message);
  void forceLossForTesting();

  // EventTarget methods
  void addEventListener(jsi::Runtime &runtime, std::string type,
                        jsi::Function callback);
  void removeEventListener(jsi::Runtime &runtime, std::string type,
                           jsi::Function callback);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &GPUDevice::getBrand);
    installMethod(runtime, prototype, "destroy", &GPUDevice::destroy);
    installMethod(runtime, prototype, "createBuffer", &GPUDevice::createBuffer);
    installMethod(runtime, prototype, "createTexture",
                  &GPUDevice::createTexture);
    installMethod(runtime, prototype, "createSampler",
                  &GPUDevice::createSampler);
    installMethod(runtime, prototype, "importExternalTexture",
                  &GPUDevice::importExternalTexture);
    installMethod(runtime, prototype, "importSharedTextureMemory",
                  &GPUDevice::importSharedTextureMemory);
    installMethod(runtime, prototype, "importSharedFence",
                  &GPUDevice::importSharedFence);
    installMethod(runtime, prototype, "createBindGroupLayout",
                  &GPUDevice::createBindGroupLayout);
    installMethod(runtime, prototype, "createPipelineLayout",
                  &GPUDevice::createPipelineLayout);
    installMethod(runtime, prototype, "createBindGroup",
                  &GPUDevice::createBindGroup);
    installMethod(runtime, prototype, "createShaderModule",
                  &GPUDevice::createShaderModule);
    installMethod(runtime, prototype, "createComputePipeline",
                  &GPUDevice::createComputePipeline);
    installMethod(runtime, prototype, "createRenderPipeline",
                  &GPUDevice::createRenderPipeline);
    installMethodWithRuntime(runtime, prototype, "createComputePipelineAsync",
                             &GPUDevice::createComputePipelineAsync);
    installMethodWithRuntime(runtime, prototype, "createRenderPipelineAsync",
                             &GPUDevice::createRenderPipelineAsync);
    installMethod(runtime, prototype, "createCommandEncoder",
                  &GPUDevice::createCommandEncoder);
    installMethod(runtime, prototype, "createRenderBundleEncoder",
                  &GPUDevice::createRenderBundleEncoder);
    installMethod(runtime, prototype, "createQuerySet",
                  &GPUDevice::createQuerySet);
    installMethod(runtime, prototype, "pushErrorScope",
                  &GPUDevice::pushErrorScope);
    installMethodWithRuntime(runtime, prototype, "popErrorScope",
                             &GPUDevice::popErrorScope);
    installGetter(runtime, prototype, "features", &GPUDevice::getFeatures);
    installGetter(runtime, prototype, "limits", &GPUDevice::getLimits);
    installGetter(runtime, prototype, "queue", &GPUDevice::getQueue);
    installGetterWithRuntime(runtime, prototype, "lost", &GPUDevice::getLost);
    installGetterSetter(runtime, prototype, "label", &GPUDevice::getLabel,
                        &GPUDevice::setLabel);
    installMethod(runtime, prototype, "forceLossForTesting",
                  &GPUDevice::forceLossForTesting);

    // EventTarget methods - installed manually since they take jsi::Function
    auto addEventListenerFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "addEventListener"), 2,
        [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args,
           size_t count) -> jsi::Value {
          if (count < 2 || !args[0].isString() || !args[1].isObject() ||
              !args[1].getObject(rt).isFunction(rt)) {
            return jsi::Value::undefined();
          }
          auto native = GPUDevice::fromValue(rt, thisVal);
          native->addEventListener(rt, args[0].getString(rt).utf8(rt),
                                   args[1].getObject(rt).getFunction(rt));
          return jsi::Value::undefined();
        });
    prototype.setProperty(runtime, "addEventListener", addEventListenerFunc);

    auto removeEventListenerFunc = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forUtf8(runtime, "removeEventListener"), 2,
        [](jsi::Runtime &rt, const jsi::Value &thisVal, const jsi::Value *args,
           size_t count) -> jsi::Value {
          if (count < 2 || !args[0].isString() || !args[1].isObject() ||
              !args[1].getObject(rt).isFunction(rt)) {
            return jsi::Value::undefined();
          }
          auto native = GPUDevice::fromValue(rt, thisVal);
          native->removeEventListener(rt, args[0].getString(rt).utf8(rt),
                                      args[1].getObject(rt).getFunction(rt));
          return jsi::Value::undefined();
        });
    prototype.setProperty(runtime, "removeEventListener",
                          removeEventListenerFunc);
  }

  inline const wgpu::Device get() { return _instance; }

private:
  friend class GPUAdapter;

  // Runs uncapturederror listeners on the owning runtime's JS thread.
  void deliverUncapturedError(wgpu::ErrorType type, std::string message);

  wgpu::Device _instance;
  std::shared_ptr<async::RuntimeContext> _async;
  std::string _label;
  // Guards the device-lost state below. In the ProcessEvents model both
  // notifyDeviceLost() (fired by Dawn during ProcessEvents) and getLost() run
  // on the owning runtime's own thread, but device destruction can also trigger
  // notifyDeviceLost() synchronously, so the mutex keeps these fields safe.
  std::mutex _lostMutex;
  std::optional<async::AsyncTaskHandle> _lostHandle;
  std::shared_ptr<GPUDeviceLostInfo> _lostInfo;
  bool _lostSettled = false;
  std::optional<async::AsyncTaskHandle::ResolveFunction> _lostResolve;

  // RuntimeContext owns a removable cleanup callback for this shared state.
  // Keeping its id prevents a foreign-thread GPUDevice destructor from being
  // the last owner of any jsi::Function.
  std::shared_ptr<RuntimeEventListeners> _eventListeners;
  async::RuntimeContext::InvalidationCallbackId _listenerCleanupCallbackId{0};
};

} // namespace rnwgpu
