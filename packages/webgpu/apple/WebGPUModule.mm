#import "WebGPUModule.h"
#include "ApplePlatformContext.h"
#import "GPUCanvasContext.h"
#include "SurfaceRegistry.h"

#import <QuartzCore/CADisplayLink.h>
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

@implementation WebGPUModule {
  CADisplayLink *_displayLink;
}

RCT_EXPORT_MODULE(WebGPUModule)

static std::shared_ptr<rnwgpu::RNWebGPUManager> webgpuManager;

// Synthesize callInvoker so RCTTurboModuleManager injects the JS CallInvoker.
// When the module conforms to RCTCallInvokerModule, the TurboModule infra
// calls setCallInvoker: during module initialization.
@synthesize callInvoker = _callInvoker;

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return webgpuManager;
}

#pragma Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

- (void)invalidate {
  [self stopDisplayLink];
  webgpuManager = nil;
  [super invalidate];
}

- (void)startDisplayLink {
  if (_displayLink != nil) {
    return;
  }
  // CADisplayLink callbacks must be scheduled on a run loop. The main run
  // loop is the safest choice: CAMetalLayer ops are main-thread-only, and
  // SurfaceInfo's mutex serialises access with the JS thread.
  dispatch_async(dispatch_get_main_queue(), ^{
    if (self->_displayLink != nil) {
      return;
    }
    self->_displayLink = [CADisplayLink displayLinkWithTarget:self
                                                     selector:@selector(onVsync:)];
    [self->_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                             forMode:NSRunLoopCommonModes];
  });
}

- (void)stopDisplayLink {
  CADisplayLink *link = _displayLink;
  _displayLink = nil;
  if (link == nil) {
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    [link invalidate];
  });
}

- (void)onVsync:(CADisplayLink *)__unused link {
  rnwgpu::SurfaceRegistry::getInstance().tickAll();
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
    NSLog(@"Failed to install react-native-wgpu: jsi::Runtime* was null! "
          @"(self.bridge=%@)",
          self.bridge);
    return [NSNumber numberWithBool:NO];
  }

  // _callInvoker is injected by RCTTurboModuleManager because we conform to
  // RCTCallInvokerModule. Works in both Legacy and Bridgeless.
  std::shared_ptr<react::CallInvoker> jsInvoker = _callInvoker.callInvoker;
  if (!jsInvoker) {
    NSLog(@"Failed to install react-native-wgpu: react::CallInvoker was "
          @"null!");
    return [NSNumber numberWithBool:NO];
  }

  std::shared_ptr<rnwgpu::PlatformContext> platformContext =
      std::make_shared<rnwgpu::ApplePlatformContext>();
  webgpuManager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsInvoker,
                                                            platformContext);
  [self startDisplayLink];
  return @true;
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}

@end
