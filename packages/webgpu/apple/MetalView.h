#pragma once

#import "RNWGUIKit.h"
#import "WebGPUModule.h"

@interface MetalView : RNWGPlatformView

@property NSNumber *contextId;

// Canvas mode (the default): the canvas renders into textures it owns and
// this view copies the latest finished one onto its layer from the main
// thread, paced by a CADisplayLink (see BlitPresenter.h). Off, the rendering
// thread presents the layer's swapchain itself (swapchain mode). Changing it
// on a configured view re-attaches the same layer the other way. macOS has no
// canvas mode yet: the flag is ignored there and the swapchain is always used.
@property(nonatomic) BOOL canvasMode;

- (void)configure;
- (void)update;

@end
