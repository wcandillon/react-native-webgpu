#import "WebGPUModule.h"
#import "GPUCanvasContext.h"
#import <React/RCTBridge+Private.h>
#import <React/RCTLog.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import <memory>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

@implementation WebGPUModule

RCT_EXPORT_MODULE(WebGPUModule)

static rnwgpu::RNWebGPUManager *webgpuManager;
static NSCondition *_condition;
static NSMutableSet *_surfaceContextsIds;

+ (rnwgpu::RNWebGPUManager *)getManager {
  return webgpuManager;
}

+ (void)onSurfaceCreated:(NSNumber *)contextId
{
  [_condition lock];
  [_surfaceContextsIds addObject:contextId];
  [_condition signal];
  [_condition unlock];
}

#pragma Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

- (instancetype)init
{
  self = [super init];
  _condition = [NSCondition new];
  return self;
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
  webgpuManager = new rnwgpu::RNWebGPUManager(runtime, jsInvoker);
  return @true;
}

//RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(createSurfaceContext:(nonnull NSNumber *)contextId) {
RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(createSurfaceContext:(double)contextId) {
//  int contextIdInt = [contextId intValue];
  int contextIdInt = contextId;
  return @(0);
  RCTCxxBridge *cxxBridge = (RCTCxxBridge *)[RCTBridge currentBridge];
  auto runtime = (jsi::Runtime *)cxxBridge.runtime;
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(
      *runtime, "__WebGPUContextRegistry");
  if (webGPUContextRegistry.hasProperty(*runtime,
                                        std::to_string(contextIdInt).c_str())) {
    // Context already exists
    return @true;
  }
  
  [_condition lock];
  while (![_surfaceContextsIds containsObject:@(contextIdInt)]) {
    [_condition wait];
  }
  [_condition unlock];
  
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
