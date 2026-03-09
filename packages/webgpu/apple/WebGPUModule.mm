#import "WebGPUModule.h"
#include "ApplePlatformContext.h"
#import "GPUCanvasContext.h"

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

static std::vector<std::string> ReadToggleArray(NSDictionary *infoDictionary,
                                                NSString *key) {
  std::vector<std::string> toggles;
  id value = infoDictionary[key];
  if (![value isKindOfClass:[NSArray class]]) {
    return toggles;
  }

  for (id item in (NSArray *)value) {
    if (![item isKindOfClass:[NSString class]]) {
      continue;
    }
    NSString *toggle = [(NSString *)item
        stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (toggle.length == 0) {
      continue;
    }
    toggles.push_back(toggle.UTF8String);
  }

  return toggles;
}

@implementation WebGPUModule

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

  // self.bridge works in both Legacy (RCTBridge) and Bridgeless (RCTBridgeProxy).
  jsi::Runtime *runtime = (jsi::Runtime *)self.bridge.runtime;
  if (!runtime) {
    NSLog(@"Failed to install react-native-wgpu: jsi::Runtime* was null! "
          @"(self.bridge=%@)", self.bridge);
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

  NSDictionary *infoDictionary = NSBundle.mainBundle.infoDictionary ?: @{};
  auto enableToggles =
      ReadToggleArray(infoDictionary, @"RNWebGPUEnableToggles");
  auto disableToggles =
      ReadToggleArray(infoDictionary, @"RNWebGPUDisableToggles");

  std::shared_ptr<rnwgpu::PlatformContext> platformContext =
      std::make_shared<rnwgpu::ApplePlatformContext>();
  webgpuManager = std::make_shared<rnwgpu::RNWebGPUManager>(
      runtime, jsInvoker, platformContext,
      std::move(enableToggles), std::move(disableToggles));
  return @true;
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}

@end
