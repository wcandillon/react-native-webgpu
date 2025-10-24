#pragma once

#import "RNWebGPUManager.h"
#import <RNWgpuViewSpec/RNWgpuViewSpec.h>
#import <React/RCTEventEmitter.h>

@interface WebGPUModule : RCTEventEmitter <NativeWebGPUModuleSpec>

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager;

@end
