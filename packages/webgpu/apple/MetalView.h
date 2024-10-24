#pragma once

#import "WebGPUModule.h"
#import "RNWGUIKit.h"

@interface MetalView : RNWGPlatformView

@property NSNumber *contextId;

- (void)configure;
- (void)update;

@end
