#pragma once

#include <atomic>
#include <chrono>
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

#include "GPUAdapterInfo.h"
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
#include "GPUNativeBufferExternalTexture.h"
#include "GPUNativeBufferTexture.h"
#include "GPUNativeBufferTextureDescriptor.h"
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
#include "VideoFrameBlit.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUDevice : public NativeObject<GPUDevice> {
public:
  static constexpr const char *CLASS_NAME = "GPUDevice";

  // Where the device came from: requestDevice(), or another library through
  // importDevice(). An imported device's pipeline cache is its owner's.
  enum class Origin { Requested, Imported };

  explicit GPUDevice(wgpu::Device instance,
                     std::shared_ptr<async::RuntimeContext> async,
                     std::string label, Origin origin)
      : NativeObject(CLASS_NAME), _instance(instance), _async(async),
        _label(label), _origin(origin) {}

  ~GPUDevice() override {
    // Unregister from the static registry
    unregisterDevice(_instance.Get());
  }

  // Static registry for looking up GPUDevice from wgpu::Device in callbacks.
  // Holds the wrapper by plain pointer: it unregisters itself in its
  // destructor, under the registry's mutex, before any of its members go, so
  // an entry's wrapper is alive for as long as the mutex is held, and its
  // weak_from_this() tells whether it may still be shared (expired once it is
  // being destroyed).
  static void registerDevice(WGPUDevice handle, GPUDevice *device) {
    std::lock_guard<std::mutex> lock(getRegistryMutex());
    getRegistry()[handle] = device;
  }

  static void unregisterDevice(WGPUDevice handle) {
    std::lock_guard<std::mutex> lock(getRegistryMutex());
    getRegistry().erase(handle);
  }

  static std::shared_ptr<GPUDevice> lookupDevice(WGPUDevice handle) {
    std::lock_guard<std::mutex> lock(getRegistryMutex());
    auto it = getRegistry().find(handle);
    if (it != getRegistry().end()) {
      return it->second->weak_from_this().lock();
    }
    return nullptr;
  }

private:
  static std::unordered_map<WGPUDevice, GPUDevice *> &getRegistry() {
    static std::unordered_map<WGPUDevice, GPUDevice *> registry;
    return registry;
  }

  static std::mutex &getRegistryMutex() {
    static std::mutex mutex;
    return mutex;
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
  // Import a native buffer once and bracket each use with beginAccess /
  // endAccess: the counterparts of createTexture / createExternalTexture in
  // GPUAndroidHardwareBufferUtil of Dawn's Android layer.
  std::shared_ptr<GPUNativeBufferTexture> createNativeBufferTexture(
      std::shared_ptr<VideoFrame> source,
      std::optional<std::shared_ptr<GPUNativeBufferTextureDescriptor>>
          descriptor);
  std::shared_ptr<GPUNativeBufferExternalTexture>
  createNativeBufferExternalTexture(
      std::shared_ptr<VideoFrame> source,
      std::optional<std::shared_ptr<GPUExternalTextureDescriptor>> descriptor);
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
  std::shared_ptr<GPUAdapterInfo> getAdapterInfo();
  std::shared_ptr<GPUQueue> getQueue();
  jsi::Value getLost(jsi::Runtime &runtime, const jsi::Object &wrapper);
  void notifyDeviceLost(wgpu::DeviceLostReason reason, std::string message);
  void notifyUncapturedError(wgpu::ErrorType type, std::string message);
  void forceLossForTesting();

  // EventTarget methods
  void addEventListener(std::string type, jsi::Function callback);
  void removeEventListener(std::string type, jsi::Function callback);

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
    installMethod(runtime, prototype, "createNativeBufferTexture",
                  &GPUDevice::createNativeBufferTexture);
    installMethod(runtime, prototype, "createNativeBufferExternalTexture",
                  &GPUDevice::createNativeBufferExternalTexture);
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
    installGetter(runtime, prototype, "adapterInfo",
                  &GPUDevice::getAdapterInfo);
    installGetter(runtime, prototype, "queue", &GPUDevice::getQueue);
    // `lost` is installed manually: the getter needs the wrapper object
    // itself, because the promise (and its resolve function) are cached as
    // hidden properties on the JS side so the GC traces them as part of the
    // device graph. Holding them strongly from C++ would root the promise's
    // .then reactions forever (issue #445).
    {
      auto lostGetter = jsi::Function::createFromHostFunction(
          runtime, jsi::PropNameID::forUtf8(runtime, "get_lost"), 0,
          [](jsi::Runtime &rt, const jsi::Value &thisVal,
             const jsi::Value * /*args*/, size_t /*count*/) -> jsi::Value {
            auto native = GPUDevice::fromValue(rt, thisVal);
            return native->getLost(rt, thisVal.getObject(rt));
          });
      auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
      auto defineProperty =
          objectCtor.getPropertyAsFunction(runtime, "defineProperty");
      jsi::Object descriptor(runtime);
      descriptor.setProperty(runtime, "get", lostGetter);
      descriptor.setProperty(runtime, "enumerable", true);
      descriptor.setProperty(runtime, "configurable", true);
      defineProperty.call(runtime, prototype,
                          jsi::String::createFromUtf8(runtime, "lost"),
                          descriptor);
    }
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
          native->addEventListener(args[0].getString(rt).utf8(rt),
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
          native->removeEventListener(args[0].getString(rt).utf8(rt),
                                      args[1].getObject(rt).getFunction(rt));
          return jsi::Value::undefined();
        });
    prototype.setProperty(runtime, "removeEventListener",
                          removeEventListenerFunc);
  }

  inline const wgpu::Device get() { return _instance; }

  // Has Dawn write what its monolithic Vulkan pipeline cache holds to the blob
  // cache (see DawnBlobCache). Dawn keeps that cache in memory and writes it
  // only when the embedder says the device is idle (the
  // vulkan_monolithic_pipeline_cache toggle says so), so without this a
  // relaunch would start with an empty one. Cheap when no pipeline was
  // compiled since the last call: Dawn keeps a flag, and skips a cache whose
  // size has not changed. Nothing to do on other backends, nor on an imported
  // device, whose owner keeps its cache, nor once the blob cache has refused
  // the pipeline cache's data as too large (see DawnBlobCache::setDirectory):
  // serializing it again would be for nothing. Runs on the calling thread,
  // which must be one that may use the device: the thread of its runtime, or
  // any thread when the device synchronizes itself
  // (ImplicitDeviceSynchronization). destroy() calls it, and so do
  // flushPipelineCacheIfDue() and flushPipelineCaches().
  void flushPipelineCache();

  // flushPipelineCache(), at most once every ten seconds. GPUQueue::submit
  // calls it, so that a session that ends without destroy() or a pause (a
  // crash, the system killing the app in the foreground) loses at most those
  // seconds of pipeline building. Serializing the pipeline cache would hold up
  // the frame being submitted (Dawn copies the whole of it out of the driver,
  // hashes it and copies it again), so a device that synchronizes itself (the
  // default) is flushed on the blob cache's thread instead; a device only its
  // runtime's thread may use is flushed here. Any thread that may use the
  // device.
  void flushPipelineCacheIfDue();

  // Flushes every live device: here when the device synchronizes itself (the
  // default), else on the thread of its runtime, which a runtime that polls
  // its mailbox (a worklet runtime's) reaches with its next operation. Any
  // thread: the platform modules call it when the app goes to the background,
  // the last moment to write before the process may be killed.
  static void flushPipelineCaches();

private:
  friend class GPUAdapter;

  // Runs the uncapturederror listeners on the creation runtime's JS thread.
  // Invoked from notifyUncapturedError through the runtime's context.
  void deliverUncapturedError(wgpu::ErrorType type, std::string message);

  wgpu::Device _instance;
  std::shared_ptr<async::RuntimeContext> _async;
  std::string _label;
  const Origin _origin;
  // Set once the blob cache refused the pipeline cache's data as too large
  // (see flushPipelineCache). Shared with the flushes on the cache's thread,
  // which hold the device by its Dawn handle and may outlive this wrapper.
  std::shared_ptr<std::atomic<bool>> _pipelineCacheOutgrown =
      std::make_shared<std::atomic<bool>>(false);
  // When flushPipelineCacheIfDue() last flushed, in steady_clock ticks. A
  // flush is claimed by swapping in the time, so that threads submitting at
  // once flush once.
  std::atomic<std::chrono::steady_clock::rep> _lastPipelineCacheFlush{
      std::chrono::steady_clock::now().time_since_epoch().count()};
  // Renders native video frames into textures for
  // GPUQueue.copyExternalImageToTexture; shared by every GPUQueue wrapper of
  // this device so the pipelines are created once.
  std::shared_ptr<VideoFrameBlit> _videoFrameBlit =
      std::make_shared<VideoFrameBlit>();
  // Guards the device-lost state below. getLost() runs on a JS thread, but
  // Dawn's AllowSpontaneous device-lost callback (and device destruction) can
  // fire notifyDeviceLost() from other threads, so the mutex keeps these
  // fields safe.
  std::mutex _lostMutex;
  std::shared_ptr<GPUDeviceLostInfo> _lostInfo;
  bool _lostSettled = false;
  // Pending `lost` promises, held WEAKLY. A strong native reference would be
  // a GC root: the promise's .then reactions (three.js captures its whole
  // renderer there) could never be collected, pinning every GPU wrapper of
  // the scene and its reported external memory forever (issue #445). The
  // resolve function lives as a hidden property on the promise object itself,
  // so if the promise is still alive when the device is lost we can settle
  // it; if it was collected, nobody could have observed the resolution.
  // Each entry is settled on the thread of the runtime that read `lost`,
  // through that runtime's context; a runtime gone since is skipped.
  struct PendingLostPromise {
    jsi::WeakObject promise;
    std::weak_ptr<async::RuntimeContext> context;
  };
  std::vector<PendingLostPromise> _lostPromises;

  // Event listeners storage - keyed by event type
  // Each entry contains a vector of shared_ptr to functions
  std::unordered_map<std::string, std::vector<std::shared_ptr<jsi::Function>>>
      _eventListeners;
};

} // namespace rnwgpu
