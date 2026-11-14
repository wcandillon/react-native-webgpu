#pragma once

#import "RNWGUIKit.h"
#import "WebGPUModule.h"

@interface MetalView : RNWGPlatformView

@property NSNumber *contextId;
// The colorSpace and toneMapping props of the Canvas, as they are when it
// mounts: set before configure, which tags the layer with them.
@property BOOL displayP3;
@property BOOL extendedDynamicRange;

- (void)configure;
- (void)update;

@end
