#ifdef RCT_NEW_ARCH_ENABLED
#pragma once

#import <React/RCTViewComponentView.h>
#import <UIKit/UIKit.h>
#import "MetalView.h"

NS_ASSUME_NONNULL_BEGIN

@interface WebGPUView : RCTViewComponentView
+ (void)registerMetalView:(MetalView *)metalView withContextId:(NSNumber *)contextId;
+ (bool)isContextRegisterd:(NSNumber *)contextId;
@end

NS_ASSUME_NONNULL_END

#endif /* RCT_NEW_ARCH_ENABLED */
