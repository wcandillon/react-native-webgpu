#include "RNWebGPUManager.h"

#include "GPU.h"
#include "NativeObject.h"
#include "RNWebGPU.h"
#include "SurfaceRegistry.h"

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
#include <stdexcept>
#include <string>
#include <utility>

namespace rnwgpu {

namespace {

struct RuntimeSessionData {
  RNWebGPUSessionId sessionId{kInvalidRNWebGPUSessionId};
};

class SessionInstallationGuard final {
public:
  SessionInstallationGuard(
      RNWebGPUSessionId sessionId,
      std::shared_ptr<RNWebGPUSessionState> sessionState) noexcept
      : _sessionId(sessionId), _sessionState(std::move(sessionState)) {}

  ~SessionInstallationGuard() {
    if (_committed) {
      return;
    }
    if (_sessionState) {
      _sessionState->invalidate();
    }
    async::RuntimeContext::unregisterMainRuntime(_sessionId);
  }

  SessionInstallationGuard(const SessionInstallationGuard &) = delete;
  SessionInstallationGuard &
  operator=(const SessionInstallationGuard &) = delete;

  void commit() noexcept { _committed = true; }

private:
  RNWebGPUSessionId _sessionId;
  std::shared_ptr<RNWebGPUSessionState> _sessionState;
  bool _committed{false};
};

jsi::UUID runtimeSessionDataUUID() {
  static constexpr jsi::UUID uuid(0x0C4D67A3, 0x944A, 0x4ECA, 0xB788,
                                  0xB86CC728D9A1ULL);
  return uuid;
}

} // namespace

RNWebGPUManager::RNWebGPUManager(
    RNWebGPUSessionId sessionId, jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
    std::shared_ptr<PlatformContext> platformContext)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker),
      _sessionState(std::make_shared<RNWebGPUSessionState>(sessionId)),
      _platformContext(platformContext) {
  if (sessionId == kInvalidRNWebGPUSessionId || _jsRuntime == nullptr ||
      !_jsCallInvoker || !_platformContext) {
    throw std::invalid_argument(
        "RNWebGPUManager requires a valid session, runtime, CallInvoker, and "
        "PlatformContext");
  }
  SessionInstallationGuard installationGuard(sessionId, _sessionState);

  auto gpu = std::make_shared<GPU>(_sessionState);
  auto rnWebGPU = std::make_shared<RNWebGPU>(
      gpu, _platformContext, _jsCallInvoker, _sessionState, *_jsRuntime);
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

  auto runtimeSessionData = std::make_shared<RuntimeSessionData>();
  runtimeSessionData->sessionId = sessionId;
  _jsRuntime->setRuntimeData(runtimeSessionDataUUID(), runtimeSessionData);

  // Register only after every JSI operation has succeeded. A throwing
  // constructor must not leave a process-global pointer to a partial session.
  async::RuntimeContext::registerMainRuntime(sessionId, _jsRuntime,
                                             _jsCallInvoker);
  installationGuard.commit();
}

RNWebGPUSessionId RNWebGPUManager::sessionForRuntime(jsi::Runtime &runtime) {
  const auto data = runtime.getRuntimeData(runtimeSessionDataUUID());
  if (!data) {
    return kInvalidRNWebGPUSessionId;
  }
  return std::static_pointer_cast<RuntimeSessionData>(data)->sessionId;
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

void RNWebGPUManager::invalidate() noexcept {
  if (_sessionState) {
    _sessionState->invalidate();
    async::RuntimeContext::unregisterMainRuntime(_sessionState->id());
  }
}

RNWebGPUManager::~RNWebGPUManager() {
  invalidate();
  _jsRuntime = nullptr;
  _jsCallInvoker.reset();
}

RNWebGPUManagerRegistry &RNWebGPUManagerRegistry::getInstance() {
  // Native views may be released during process-wide static teardown. Keep the
  // registry alive until process exit so late view destructors never observe a
  // destroyed mutex.
  static auto *registry = new RNWebGPUManagerRegistry();
  return *registry;
}

RNWebGPUSessionId RNWebGPUManagerRegistry::createSession() {
  std::lock_guard<std::mutex> lock(_mutex);
  for (;;) {
    const auto candidate = _nextSessionId++;
    if (_nextSessionId > kMaxRNWebGPUSessionId) {
      _nextSessionId = 1;
    }
    if (candidate != kInvalidRNWebGPUSessionId &&
        _entries.find(candidate) == _entries.end()) {
      return candidate;
    }
  }
}

void RNWebGPUManagerRegistry::publish(
    RNWebGPUSessionId sessionId, std::shared_ptr<RNWebGPUManager> manager) {
  if (sessionId == kInvalidRNWebGPUSessionId || !manager ||
      manager->sessionId() != sessionId) {
    throw std::invalid_argument("Cannot publish an invalid WebGPU session");
  }

  std::unique_lock<std::mutex> lock(_mutex);
  if (_entries.find(sessionId) != _entries.end()) {
    throw std::logic_error("WebGPU session was already published");
  }

  const auto supersededSessionId = _activeSessionId;
  std::shared_ptr<RNWebGPUManager> supersededManager;
  if (supersededSessionId != kInvalidRNWebGPUSessionId &&
      supersededSessionId != sessionId) {
    const auto superseded = _entries.find(supersededSessionId);
    if (superseded != _entries.end()) {
      supersededManager = superseded->second.manager;
    }
  }

  SurfaceRegistry::getInstance().openSession(sessionId);
  try {
    _entries.emplace(sessionId, Entry{std::move(manager), 1});
    // Publication is the generation boundary. Invalidate the old token before
    // exposing the new active id so a long-lived worklet cannot use a boxed GPU
    // from the previous runtime to replace the new RuntimeContext.
    if (supersededManager) {
      supersededManager->invalidate();
    }
    _activeSessionId = sessionId;
  } catch (...) {
    lock.unlock();
    SurfaceRegistry::getInstance().closeSession(sessionId);
    throw;
  }
  lock.unlock();

  if (supersededSessionId != kInvalidRNWebGPUSessionId &&
      supersededSessionId != sessionId) {
    // The old lease remains in _entries until its module owners release it,
    // but its native surfaces become terminal at the generation boundary.
    SurfaceRegistry::getInstance().closeSession(supersededSessionId);
  }
}

RNWebGPUManagerSnapshot
RNWebGPUManagerRegistry::acquire(RNWebGPUSessionId sessionId) {
  std::lock_guard<std::mutex> lock(_mutex);
  const auto it = _entries.find(sessionId);
  if (it == _entries.end() || !it->second.manager ||
      !it->second.manager->isActive()) {
    return {};
  }
  ++it->second.owners;
  return {sessionId, it->second.manager};
}

RNWebGPUManagerSnapshot RNWebGPUManagerRegistry::getActive() const {
  std::lock_guard<std::mutex> lock(_mutex);
  const auto it = _entries.find(_activeSessionId);
  if (it == _entries.end() || !it->second.manager ||
      !it->second.manager->isActive()) {
    return {};
  }
  return {_activeSessionId, it->second.manager};
}

RNWebGPUManagerSnapshot
RNWebGPUManagerRegistry::get(RNWebGPUSessionId sessionId) const {
  std::lock_guard<std::mutex> lock(_mutex);
  const auto it = _entries.find(sessionId);
  if (it == _entries.end() || !it->second.manager ||
      !it->second.manager->isActive()) {
    return {};
  }
  return {sessionId, it->second.manager};
}

std::shared_ptr<RNWebGPUManager>
RNWebGPUManagerRegistry::release(RNWebGPUSessionId sessionId) {
  std::shared_ptr<RNWebGPUManager> releasedManager;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _entries.find(sessionId);
    if (it == _entries.end()) {
      return nullptr;
    }
    if (it->second.owners > 1) {
      --it->second.owners;
      return nullptr;
    }

    releasedManager = std::move(it->second.manager);
    _entries.erase(it);
    if (_activeSessionId == sessionId) {
      _activeSessionId = kInvalidRNWebGPUSessionId;
    }
  }

  if (releasedManager) {
    releasedManager->invalidate();
  }
  SurfaceRegistry::getInstance().closeSession(sessionId);
  return releasedManager;
}

} // namespace rnwgpu
