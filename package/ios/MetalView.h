#pragma once

#import <UIKit/UIKit.h>
#import "WebGPUModule.h"

@interface MetalView : UIView

- (void)setContectId:(NSNumber *)contextId webGPUModule:(WebGPUModule *)webGPUModule;

@end
