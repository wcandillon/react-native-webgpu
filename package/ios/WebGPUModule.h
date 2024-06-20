#pragma once

#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>

@interface WebGPUModule : NSObject <RCTBridgeModule>

@property(nonatomic, weak) RCTBridge* bridge;
@property(nonatomic, weak) RCTModuleRegistry* moduleRegistry;

@end

#ifdef RCT_NEW_ARCH_ENABLED
#import <rnwebgpu/rnwebgpu.h>

@interface WebGPUModule () <WebGPUModuleSpec>
@end
#endif