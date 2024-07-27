#import "WebGPUModule.h"
#import "GPUCanvasContext.h"
#import <React/RCTBridge+Private.h>
#import <React/RCTLog.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import <memory>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

@implementation WebGPUModule {
  rnwgpu::RNWebGPUManager *_webgpuManager;
}

RCT_EXPORT_MODULE(WebGPUModule)

#pragma Accessors

- (rnwgpu::RNWebGPUManager *)manager {
  return _webgpuManager;
}

#pragma Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

- (void)invalidate {
  // if (_webgpuManager != nil) {
  //   [_webgpuManager invalidate];
  // }
  _webgpuManager = nil;
}

- (rnwgpu::RNWebGPUManager *)getManager {
  return _webgpuManager;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  if (_webgpuManager != nil) {
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
  _webgpuManager = new rnwgpu::RNWebGPUManager(runtime, jsInvoker);
  return @true;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(createSurfaceContext
                                       : (nonnull NSNumber *)contextId) {
  int contextIdInt = [contextId intValue];
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)[RCTBridge currentBridge];
  auto runtime = (jsi::Runtime *)cxxBridge.runtime;
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(
      *runtime, "__WebGPUContextRegistry");
  if (webGPUContextRegistry.hasProperty(*runtime,
                                        std::to_string(contextIdInt).c_str())) {
    // Context already exists
    return @true;
  }

  auto surfaceData = _webgpuManager->surfacesRegistry.getSurface(contextIdInt);
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
  jsInvoker = params.jsInvoker;
  return std::make_shared<facebook::react::NativeSkiaModuleSpecJSI>(params);
}
#endif

@end
