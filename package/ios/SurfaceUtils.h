#pragma once

#import <UIKit/UIKit.h>

@interface SurfaceUtils : NSObject

+ (void)configureSurface:(CALayer *)layer
                    size:(CGSize)size
               contextId:(int)contextId;

@end
