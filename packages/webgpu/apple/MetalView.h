#pragma once

#import "RNWGUIKit.h"
#import "WebGPUModule.h"

@interface MetalView : RNWGPlatformView

@property NSNumber *contextId;

- (void)configure;
- (void)update;
- (void)startPresentLoop;
- (void)stopPresentLoop;

@end
