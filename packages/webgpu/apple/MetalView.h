#pragma once

#import "RNWGUIKit.h"
#import "WebGPUModule.h"

@interface MetalView : RNWGPlatformView

@property NSNumber *contextId;

// Present with a copy from the UI thread (see BlitPresenter.h) instead of
// letting the rendering thread present the layer's swapchain. Changing it on
// a configured view re-attaches the same layer the other way. Not supported
// on macOS yet, where it is ignored.
@property(nonatomic) BOOL presentsWithCopy;

- (void)configure;
- (void)update;

@end
