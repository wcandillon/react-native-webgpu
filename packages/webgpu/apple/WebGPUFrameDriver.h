#pragma once

#import <Foundation/Foundation.h>

// Objective-C wrapper around the platform vsync source (CADisplayLink) that
// drives rnwgpu::FrameDriver::onVSync() once per frame. start/stop are invoked
// by the C++ FrameDriver via setPlatformVSync; both hop to the main thread.
@interface WebGPUFrameDriver : NSObject

+ (void)start;
+ (void)stop;

@end
