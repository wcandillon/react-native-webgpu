#import "WebGPUModule.h"
#import "GPUCanvasContext.h"
#import <React/RCTBridge+Private.h>
#import <React/RCTLog.h>
#import <React/RCTUIManagerUtils.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import <memory>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

@implementation WebGPUModule

RCT_EXPORT_MODULE(WebGPUModule)

static std::shared_ptr<rnwgpu::RNWebGPUManager> webgpuManager;

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
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)[RCTBridge currentBridge];
  if (!cxxBridge.runtime) {
    NSLog(@"Failed to install react-native-filament: RCTBridge is not a "
          @"RCTCxxBridge!");
    return [NSNumber numberWithBool:NO];
  }

  jsi::Runtime *runtime = (jsi::Runtime *)cxxBridge.runtime;
  if (!runtime) {
    NSLog(@"Failed to install react-native-filament: jsi::Runtime* was null!");
    return [NSNumber numberWithBool:NO];
  }
  std::shared_ptr<react::CallInvoker> jsInvoker = cxxBridge.jsCallInvoker;
  if (!jsInvoker) {
    NSLog(@"Failed to install react-native-filament: react::CallInvoker was "
          @"null!");
    return [NSNumber numberWithBool:NO];
  }

  if (!jsInvoker) {
    jsInvoker = cxxBridge.jsCallInvoker;
  }
  // TODO: remove allocation here
  webgpuManager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsInvoker);
  return @true;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(createSurfaceContext
                                       : (double)contextId) {
  int contextIdInt = contextId;
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)self.bridge;
  auto runtime = (jsi::Runtime *)cxxBridge.runtime;
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(
      *runtime, "__WebGPUContextRegistry");
  if (webGPUContextRegistry.hasProperty(*runtime,
                                        std::to_string(contextIdInt).c_str())) {
    // Context already exists
    return @true;
  }

  auto surfaceData = webgpuManager->surfacesRegistry.getSurface(contextIdInt);
  auto label = "Context: " + std::to_string(contextIdInt);
  auto gpuCanvasContext =
      std::make_shared<rnwgpu::GPUCanvasContext>(*surfaceData);
  auto gpuCanvasContextJs =
      facebook::jsi::Object::createFromHostObject(*runtime, gpuCanvasContext);
  webGPUContextRegistry.setProperty(
      *runtime, std::to_string(contextIdInt).c_str(), gpuCanvasContextJs);

  return @true;
}

#ifdef RCT_NEW_ARCH_ENABLED
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}
#endif

@end
