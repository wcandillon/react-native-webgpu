#pragma once

#import "RNWebGPUManager.h"
#import <RNWgpuViewSpec/RNWgpuViewSpec.h>
#import <React/RCTCallInvokerModule.h>
#import <React/RCTEventEmitter.h>

@interface WebGPUModule
    : RCTEventEmitter <NativeWebGPUModuleSpec, RCTCallInvokerModule>

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager;

@end
