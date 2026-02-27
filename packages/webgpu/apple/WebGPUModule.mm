#import "WebGPUModule.h"
#include "ApplePlatformContext.h"
#import "GPUCanvasContext.h"

#import <React/RCTBridge+Private.h>
#import <React/RCTLog.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import <memory>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

@implementation WebGPUModule {
  std::shared_ptr<react::CallInvoker> _callInvoker;
}

RCT_EXPORT_MODULE(WebGPUModule)

static std::shared_ptr<rnwgpu::RNWebGPUManager> webgpuManager;

#pragma mark - RCTCallInvokerModule

- (void)setCallInvoker:(std::shared_ptr<react::CallInvoker>)callInvoker {
  _callInvoker = callInvoker;
}

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return webgpuManager;
}

#pragma Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

- (void)invalidate {
  // if (_webgpuManager != nil) {
  //   [_webgpuManager invalidate];
  // }
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

  RCTBridge *bridge = self.bridge;
  if (!bridge) {
    NSLog(@"Failed to install react-native-wgpu: RCTBridge was nil!");
    return [NSNumber numberWithBool:NO];
  }

  // In Bridgeless mode, self.bridge is an RCTBridgeProxy which exposes runtime
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)bridge;
  jsi::Runtime *runtime = (jsi::Runtime *)cxxBridge.runtime;
  if (!runtime) {
    NSLog(@"Failed to install react-native-wgpu: jsi::Runtime* was null!");
    return [NSNumber numberWithBool:NO];
  }

  if (!_callInvoker) {
    NSLog(@"Failed to install react-native-wgpu: react::CallInvoker was "
          @"null!");
    return [NSNumber numberWithBool:NO];
  }

  std::shared_ptr<rnwgpu::PlatformContext> platformContext =
      std::make_shared<rnwgpu::ApplePlatformContext>();
  webgpuManager =
      std::make_shared<rnwgpu::RNWebGPUManager>(runtime, _callInvoker, platformContext);
  return @true;
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  _callInvoker = params.jsInvoker;
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}

@end
