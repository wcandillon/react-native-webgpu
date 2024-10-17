#pragma once

#import <QuartzCore/QuartzCore.h>

@interface SurfaceUtils : NSObject

+ (void)configureSurface:(CALayer *)layer
                    size:(CGSize)size
               contextId:(int)contextId;

+ (void)updateSurface:(int)contextId size:(CGSize)size;

+ (void)cleanupSurface:(int)contextId;

@end
