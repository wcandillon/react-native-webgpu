#pragma once

#import "WebGPUModule.h"
#import <UIKit/UIKit.h>

@interface MetalView : UIView

@property NSNumber *contextId;

- (void)configure;
- (void)update;

@end
