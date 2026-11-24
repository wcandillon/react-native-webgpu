#import "WebGPUModule.h"
#include "ApplePlatformContext.h"
#include "DawnBlobCache.h"
#import "GPUCanvasContext.h"
#include "GPUDevice.h"
#import "RNWGUIKit.h"

#import <React/RCTBridge+Private.h>
#import <React/RCTCallInvoker.h>
#import <React/RCTLog.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import <memory>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

// Category to declare the runtime property on RCTBridge/RCTBridgeProxy.
// In Bridgeless mode, self.bridge is an RCTBridgeProxy which implements
// -(void *)runtime. In Legacy mode, self.bridge is the real RCTBridge
// (backed by RCTCxxBridge) which also implements it.
@interface RCTBridge (JSIRuntime)
- (void *)runtime;
@end

#if !TARGET_OS_OSX
// Has Dawn free the memory it keeps but does not need (see
// GPUDevice::reduceMemoryUsage), then again while some of it waits on GPU work
// in flight, as Chrome does, a few times at most. Main thread.
static bool reducingDawnMemoryUsage = false;

static void reduceDawnMemoryUsagePass(int passesLeft) {
  if (rnwgpu::GPUDevice::reduceMemoryUsage() && passesLeft > 1) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{
                     reduceDawnMemoryUsagePass(passesLeft - 1);
                   });
    return;
  }
  reducingDawnMemoryUsage = false;
}

static void reduceDawnMemoryUsage() {
  if (reducingDawnMemoryUsage) {
    return;
  }
  reducingDawnMemoryUsage = true;
  reduceDawnMemoryUsagePass(4);
}

// A memory warning, and the move to the background, where iOS kills the apps
// that hold the most memory first: the moments Flutter and Chrome give back
// the GPU memory they can.
static void observeMemoryPressure() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    for (NSNotificationName name in @[
           UIApplicationDidReceiveMemoryWarningNotification,
           UIApplicationDidEnterBackgroundNotification
         ]) {
      [center addObserverForName:name
                          object:nil
                           queue:NSOperationQueue.mainQueue
                      usingBlock:^(NSNotification *) {
                        reduceDawnMemoryUsage();
                      }];
    }
  });
}
#endif

@implementation WebGPUModule

RCT_EXPORT_MODULE(WebGPUModule)

static std::shared_ptr<rnwgpu::RNWebGPUManager> webgpuManager;

// Synthesize callInvoker so RCTTurboModuleManager injects the JS CallInvoker.
// When the module conforms to RCTCallInvokerModule, the TurboModule infra
// calls setCallInvoker: during module initialization.
@synthesize callInvoker = _callInvoker;

// Synthesize the view registry so the TurboModule infra injects it too. It
// resolves React tags to native views in both Bridge and Bridgeless mode
// (Fabric component views included), which drawElementImageToTexture needs.
@synthesize viewRegistry_DEPRECATED = _viewRegistry_DEPRECATED;

// Synthesize the module registry so the TurboModule infra injects it too. It
// reaches React Native's BlobModule in both Bridge and Bridgeless mode.
@synthesize moduleRegistry = _moduleRegistry;

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return webgpuManager;
}

#pragma mark - Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

#if TARGET_OS_OSX
- (instancetype)init {
  if (self = [super init]) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillTerminate:)
               name:NSApplicationWillTerminateNotification
             object:nil];
  }
  return self;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  // AppKit exits without necessarily invalidating the React instance. Release
  // the manager before static teardown destroys SurfaceRegistry and its mutex.
  [self invalidate];
}
#endif

- (void)invalidate {
#if TARGET_OS_OSX
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSApplicationWillTerminateNotification
              object:nil];
#endif
  webgpuManager = nil;
}

- (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return webgpuManager;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  if (webgpuManager != nil) {
    // Already initialized, ignore call.
    return @true;
  }

  // self.bridge works in both Legacy (RCTBridge) and Bridgeless
  // (RCTBridgeProxy).
  jsi::Runtime *runtime = (jsi::Runtime *)self.bridge.runtime;
  if (!runtime) {
    NSLog(@"Failed to install react-native-webgpu: jsi::Runtime* was null! "
          @"(self.bridge=%@)",
          self.bridge);
    return [NSNumber numberWithBool:NO];
  }

  // _callInvoker is injected by RCTTurboModuleManager because we conform to
  // RCTCallInvokerModule. Works in both Legacy and Bridgeless.
  std::shared_ptr<react::CallInvoker> jsInvoker = _callInvoker.callInvoker;
  if (!jsInvoker) {
    NSLog(@"Failed to install react-native-webgpu: react::CallInvoker was "
          @"null!");
    return [NSNumber numberWithBool:NO];
  }

  // Main-thread only (see ApplePlatformContext::ViewLookup). Weak: the module
  // is invalidated on reload, after which lookups simply fail.
  __weak WebGPUModule *weakSelf = self;
  rnwgpu::ApplePlatformContext::ViewLookup viewLookup =
      [weakSelf](int tag) -> void * {
    WebGPUModule *strongSelf = weakSelf;
    if (strongSelf == nil) {
      return nullptr;
    }
    RNWGPlatformView *view =
        [strongSelf.viewRegistry_DEPRECATED viewForReactTag:@(tag)];
    return (__bridge void *)view;
  };
  rnwgpu::ApplePlatformContext::BlobManagerLookup blobManagerLookup =
      [weakSelf]() -> void * {
    WebGPUModule *strongSelf = weakSelf;
    if (strongSelf == nil) {
      return nullptr;
    }
    return (__bridge void *)[strongSelf.moduleRegistry
        moduleForName:"BlobModule"];
  };
  std::shared_ptr<rnwgpu::PlatformContext> platformContext =
      std::make_shared<rnwgpu::ApplePlatformContext>(
          std::move(viewLookup), std::move(blobManagerLookup));
  // Dawn keeps the pipelines it compiles here, so a relaunch does not compile
  // them again (see DawnBlobCache). The caches directory is per app on iOS but
  // shared on macOS, hence the bundle identifier.
  NSString *caches = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                         NSUserDomainMask, YES)
                         .firstObject;
  NSString *bundle = NSBundle.mainBundle.bundleIdentifier;
  if (caches != nil) {
    if (bundle != nil) {
      caches = [caches stringByAppendingPathComponent:bundle];
    }
    rnwgpu::DawnBlobCache::setDirectory(
        [caches stringByAppendingPathComponent:@"webgpu-dawn"].UTF8String);
  }
  webgpuManager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsInvoker,
                                                            platformContext);
#if !TARGET_OS_OSX
  observeMemoryPressure();
#endif
  return @true;
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}

@end
