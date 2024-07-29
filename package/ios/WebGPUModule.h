#pragma once

#include "RNWebGPUManager.h"
#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>

#ifdef RCT_NEW_ARCH_ENABLED
#import <RNWgpuViewSpec/RNWgpuViewSpec.h>
@interface WebGPUModule : RCTEventEmitter<NativeWebGPUModuleSpec>
#else
@interface WebGPUModule : RCTEventEmitter<RCTBridgeModule>
#endif

@property(nonatomic, weak) RCTBridge *bridge;
@property(nonatomic, weak) RCTModuleRegistry *moduleRegistry;

+ (rnwgpu::RNWebGPUManager *)getManager;
+ (void)onSurfaceCreated:(NSNumber *)contextId;

@end
