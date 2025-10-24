#pragma once

#import "RNWebGPUManager.h"
#import <React/RCTEventEmitter.h>
#import <RNWgpuViewSpec/RNWgpuViewSpec.h>

@interface WebGPUModule : RCTEventEmitter <NativeWebGPUModuleSpec>

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager;

@end
