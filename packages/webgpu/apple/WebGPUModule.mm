#import "WebGPUModule.h"
#include "ApplePlatformContext.h"
#import "GPUCanvasContext.h"

#import <React/RCTBridge+Private.h>
#import <React/RCTCallInvoker.h>
#import <React/RCTLog.h>
#import <ReactCommon/RCTTurboModule.h>
#include <condition_variable>
#include <exception>
#import <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <utility>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

namespace {

class InstallOperationGuard final {
public:
  InstallOperationGuard(std::mutex &mutex, std::condition_variable &condition,
                        bool &installInProgress) noexcept
      : _mutex(mutex), _condition(condition),
        _installInProgress(installInProgress) {}

  ~InstallOperationGuard() {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _installInProgress = false;
    }
    _condition.notify_all();
  }

  InstallOperationGuard(const InstallOperationGuard &) = delete;
  InstallOperationGuard &operator=(const InstallOperationGuard &) = delete;
  InstallOperationGuard(InstallOperationGuard &&) = delete;
  InstallOperationGuard &operator=(InstallOperationGuard &&) = delete;

private:
  std::mutex &_mutex;
  std::condition_variable &_condition;
  bool &_installInProgress;
};

} // namespace

// Category to declare the runtime property on RCTBridge/RCTBridgeProxy.
// In Bridgeless mode, self.bridge is an RCTBridgeProxy which implements
// -(void *)runtime. In Legacy mode, self.bridge is the real RCTBridge
// (backed by RCTCxxBridge) which also implements it.
@interface RCTBridge (JSIRuntime)
- (void *)runtime;
@end

@implementation WebGPUModule {
  std::mutex _managerMutex;
  std::condition_variable _managerCondition;
  bool _installInProgress;
  bool _invalidated;
  rnwgpu::RNWebGPUSessionId _ownedSessionId;
  std::shared_ptr<rnwgpu::RNWebGPUManager> _ownedManager;
}

RCT_EXPORT_MODULE(WebGPUModule)

// Synthesize callInvoker so RCTTurboModuleManager injects the JS CallInvoker.
// When the module conforms to RCTCallInvokerModule, the TurboModule infra
// calls setCallInvoker: during module initialization.
@synthesize callInvoker = _callInvoker;

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return rnwgpu::RNWebGPUManagerRegistry::getInstance().getActive().manager;
}

#pragma mark - Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

- (void)invalidate {
  rnwgpu::RNWebGPUSessionId sessionId = rnwgpu::kInvalidRNWebGPUSessionId;
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager;
  {
    std::unique_lock<std::mutex> lock(_managerMutex);
    _invalidated = true;
    _managerCondition.wait(lock, [self] { return !self->_installInProgress; });
    sessionId =
        std::exchange(_ownedSessionId, rnwgpu::kInvalidRNWebGPUSessionId);
    manager = std::move(_ownedManager);
  }

  if (sessionId != rnwgpu::kInvalidRNWebGPUSessionId) {
    try {
      rnwgpu::RNWebGPUManagerRegistry::getInstance().release(sessionId);
    } catch (const std::exception &error) {
      NSLog(@"Failed to tear down react-native-webgpu: %s", error.what());
    } catch (...) {
      NSLog(@"Failed to tear down react-native-webgpu: unknown native error");
    }
  }
  manager.reset();

  // Mark the native session inactive and detach its dispatcher before React
  // Native is allowed to start destroying the JSI runtime.
  [super invalidate];
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  // self.bridge works in both Legacy (RCTBridge) and Bridgeless
  // (RCTBridgeProxy).
  jsi::Runtime *runtime = (jsi::Runtime *)self.bridge.runtime;
  if (!runtime) {
    NSLog(@"Failed to install react-native-webgpu: jsi::Runtime* was null! "
          @"(self.bridge=%@)",
          self.bridge);
    return [NSNumber numberWithBool:NO];
  }

  auto &registry = rnwgpu::RNWebGPUManagerRegistry::getInstance();
  rnwgpu::RNWebGPUSessionId runtimeSessionId;
  rnwgpu::RNWebGPUSessionId previousSessionId;
  std::shared_ptr<rnwgpu::RNWebGPUManager> previousManager;
  {
    std::unique_lock<std::mutex> lock(_managerMutex);
    _managerCondition.wait(lock, [self] {
      return self->_invalidated || !self->_installInProgress;
    });
    if (_invalidated) {
      return @false;
    }

    runtimeSessionId = rnwgpu::RNWebGPUManager::sessionForRuntime(*runtime);
    if (_ownedManager && _ownedManager->isActive() &&
        _ownedSessionId == runtimeSessionId) {
      // This module already owns the runtime's lease. Do not acquire it again.
      return @true;
    }

    previousSessionId =
        std::exchange(_ownedSessionId, rnwgpu::kInvalidRNWebGPUSessionId);
    previousManager = std::move(_ownedManager);
    _installInProgress = true;
  }
  InstallOperationGuard installGuard(_managerMutex, _managerCondition,
                                     _installInProgress);

  try {
    if (previousSessionId != rnwgpu::kInvalidRNWebGPUSessionId) {
      registry.release(previousSessionId);
    }
    previousManager.reset();

    if (runtimeSessionId != rnwgpu::kInvalidRNWebGPUSessionId) {
      auto existing = registry.acquire(runtimeSessionId);
      if (existing) {
        bool attached = false;
        {
          std::lock_guard<std::mutex> lock(_managerMutex);
          if (!_invalidated) {
            _ownedSessionId = existing.sessionId;
            _ownedManager = existing.manager;
            attached = true;
          }
        }

        if (!attached) {
          // Releasing may tear down Dawn and surfaces, so it must remain
          // outside _managerMutex.
          registry.release(existing.sessionId);
        }
        return [NSNumber numberWithBool:attached];
      }
    }

    // _callInvoker is injected by RCTTurboModuleManager because we conform to
    // RCTCallInvokerModule. Works in both Legacy and Bridgeless.
    std::shared_ptr<react::CallInvoker> jsInvoker = _callInvoker.callInvoker;
    if (!jsInvoker) {
      NSLog(@"Failed to install react-native-webgpu: react::CallInvoker was "
            @"null!");
      return @false;
    }

    const auto sessionId = registry.createSession();
    auto platformContext = std::make_shared<rnwgpu::ApplePlatformContext>();
    auto manager = std::make_shared<rnwgpu::RNWebGPUManager>(
        sessionId, runtime, std::move(jsInvoker), std::move(platformContext));

    {
      std::lock_guard<std::mutex> lock(_managerMutex);
      if (_invalidated) {
        return @false;
      }

      // Keep the invalidation state check and publication in one critical
      // section. invalidate either wins before this point (and we do not
      // publish), or runs afterwards and releases this exact lease.
      registry.publish(sessionId, manager);
      _ownedSessionId = sessionId;
      _ownedManager = std::move(manager);
    }
    return @true;
  } catch (const std::exception &error) {
    NSLog(@"Failed to install react-native-webgpu: %s", error.what());
  } catch (...) {
    NSLog(@"Failed to install react-native-webgpu: unknown native error");
  }
  return @false;
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}

@end
