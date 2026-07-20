#pragma once

#import "RNWGUIKit.h"
#import "WebGPUModule.h"

@interface MetalView : RNWGPlatformView

@property(nonatomic, strong) NSNumber *sessionId;
@property(nonatomic, strong) NSNumber *contextId;

- (void)configure;
- (void)update;

@end
