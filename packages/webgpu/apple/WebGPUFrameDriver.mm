#import "WebGPUFrameDriver.h"

#import "RNWGUIKit.h"
#import <QuartzCore/QuartzCore.h>

#include "FrameDriver.h"

@implementation WebGPUFrameDriver

+ (void)onFrame {
  rnwgpu::FrameDriver::getInstance().onVSync();
}

#if !TARGET_OS_OSX

// iOS / tvOS: CADisplayLink on the main run loop, paused/resumed for
// start/stop.
static CADisplayLink *sDisplayLink = nil;

+ (void)tick:(CADisplayLink *)link {
  [WebGPUFrameDriver onFrame];
}

+ (void)start {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (sDisplayLink == nil) {
      sDisplayLink = [CADisplayLink displayLinkWithTarget:self
                                                 selector:@selector(tick:)];
      [sDisplayLink addToRunLoop:[NSRunLoop mainRunLoop]
                         forMode:NSRunLoopCommonModes];
    }
    sDisplayLink.paused = NO;
  });
}

+ (void)stop {
  dispatch_async(dispatch_get_main_queue(), ^{
    sDisplayLink.paused = YES;
  });
}

#else // TARGET_OS_OSX

// macOS: CADisplayLink is available via NSScreen on 14.0+. On older systems we
// fall back to an NSTimer at ~60Hz (not vsync-aligned, but keeps auto-present
// working). FrameDriver self-idles cheaply when nothing is rendering.
static id sDisplayLink = nil;

+ (void)tick:(id)sender {
  [WebGPUFrameDriver onFrame];
}

+ (void)start {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (sDisplayLink == nil) {
      if (@available(macOS 14.0, *)) {
        CADisplayLink *link =
            [NSScreen.mainScreen displayLinkWithTarget:self
                                              selector:@selector(tick:)];
        [link addToRunLoop:[NSRunLoop mainRunLoop]
                   forMode:NSRunLoopCommonModes];
        sDisplayLink = link;
      } else {
        sDisplayLink = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                                        target:self
                                                      selector:@selector(tick:)
                                                      userInfo:nil
                                                       repeats:YES];
      }
    }
    if ([sDisplayLink isKindOfClass:[CADisplayLink class]]) {
      ((CADisplayLink *)sDisplayLink).paused = NO;
    }
  });
}

+ (void)stop {
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([sDisplayLink isKindOfClass:[CADisplayLink class]]) {
      ((CADisplayLink *)sDisplayLink).paused = YES;
    }
    // NSTimer fallback keeps firing; onVSync is a cheap no-op while idle.
  });
}

#endif // TARGET_OS_OSX

@end
