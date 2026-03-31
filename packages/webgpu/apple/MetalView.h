#pragma once

#import "RNWGUIKit.h"
#import "WebGPUModule.h"

@interface MetalView : RNWGPlatformView

@property NSNumber *contextId;
@property BOOL isAttached;

- (void)configure;
- (void)update;

@end
