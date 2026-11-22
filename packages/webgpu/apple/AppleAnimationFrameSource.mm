#include "AnimationFrames.h"

#import "RNWGUIKit.h"
#import <QuartzCore/QuartzCore.h>

#include <atomic>
#include <memory>

namespace rnwgpu {
class AppleAnimationFrameSource;
std::shared_ptr<AnimationFrameSource> makeAppleAnimationFrameSource();
} // namespace rnwgpu

// The display link's target. A CADisplayLink retains its target, and the
// source outlives it anyway: the target borrows the source.
@interface RNWGAnimationFrameTarget : NSObject
- (instancetype)initWithSource:(rnwgpu::AppleAnimationFrameSource *)source;
- (void)onFrame:(CADisplayLink *)link;
@end

namespace rnwgpu {

#if !TARGET_OS_OSX

/**
 * The display's frames on iOS: a CADisplayLink on a run loop thread of its
 * own, so that a busy main thread never delays them, asking for the highest
 * rate the display has. React Native's own display link sets no frame rate
 * range, which keeps it at 60 Hz on a ProMotion iPhone. (The app must still
 * opt in to more than 60 Hz on iPhone with CADisableMinimumFrameDurationOnPhone
 * in its Info.plist.)
 *
 * A request is delivered at the next frame of the link. The link keeps running
 * between requests, and only pauses after kIdleFramesBeforePause frames
 * without one: pausing it at every frame and unpausing it at the next request
 * reads to the system as a link that keeps stopping, which it does not run at
 * 120 Hz. The frame time is the link's timestamp, moved from its clock to
 * std::chrono::steady_clock. The thread lives as long as the process.
 */
class AppleAnimationFrameSource : public AnimationFrameSource {
public:
  static std::shared_ptr<AppleAnimationFrameSource> shared() {
    static std::shared_ptr<AppleAnimationFrameSource> source = create();
    return source;
  }

  void requestFrame() override {
    _wanted.store(true);
    // Paired with onFrame(), which stores _paused before it reads _wanted:
    // either it sees this request, or this sees the link paused.
    if (_paused.load()) {
      unpause();
    }
  }

  // The link's thread.
  void onFrame(CADisplayLink *link) {
    if (!_wanted.exchange(false)) {
      if (++_idleFrames >= kIdleFramesBeforePause) {
        link.paused = YES;
        _paused.store(true);
        if (_wanted.load()) {
          unpause();
        }
      }
      return;
    }
    _idleFrames = 0;
    const CFTimeInterval age = CACurrentMediaTime() - link.timestamp;
    const int64_t ageNanos = age > 0 ? static_cast<int64_t>(age * 1e9) : 0;
    deliverFrame(AnimationFrameDispatcher::nowNanos() - ageNanos);
  }

private:
  AppleAnimationFrameSource() = default;

  static std::shared_ptr<AppleAnimationFrameSource> create() {
    auto source = std::shared_ptr<AppleAnimationFrameSource>(
        new AppleAnimationFrameSource());
    auto *raw = source.get();
    dispatch_semaphore_t ready = dispatch_semaphore_create(0);
    NSThread *thread = [[NSThread alloc] initWithBlock:^{
      RNWGAnimationFrameTarget *target =
          [[RNWGAnimationFrameTarget alloc] initWithSource:raw];
      CADisplayLink *link =
          [CADisplayLink displayLinkWithTarget:target
                                      selector:@selector(onFrame:)];
      if (@available(iOS 15.0, tvOS 15.0, *)) {
        // Up to what the display can do: the system lowers it as it sees fit
        // (Low Power Mode, a display that has no more).
        link.preferredFrameRateRange = CAFrameRateRangeMake(60, 120, 120);
      }
      link.paused = YES;
      [link addToRunLoop:[NSRunLoop currentRunLoop]
                 forMode:NSRunLoopCommonModes];
      raw->_link = link;
      raw->_runLoop = CFRunLoopGetCurrent();
      CFRetain(raw->_runLoop);
      dispatch_semaphore_signal(ready);
      // The display link is a source of the run loop: it keeps running.
      for (;;) {
        @autoreleasepool {
          [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                   beforeDate:[NSDate distantFuture]];
        }
      }
    }];
    thread.name = @"RNWebGPU Frames";
    thread.qualityOfService = NSQualityOfServiceUserInteractive;
    [thread start];
    dispatch_semaphore_wait(ready, DISPATCH_TIME_FOREVER);
    return source;
  }

  // Any thread: the link runs again from its next frame.
  void unpause() {
    CFRunLoopPerformBlock(_runLoop, kCFRunLoopCommonModes, ^{
      if (_paused.exchange(false)) {
        _link.paused = NO;
      }
    });
    CFRunLoopWakeUp(_runLoop);
  }

  static constexpr int kIdleFramesBeforePause = 8;

  CADisplayLink *_link = nil;
  CFRunLoopRef _runLoop = nullptr;
  std::atomic<bool> _wanted{false};
  std::atomic<bool> _paused{true};
  // The link's thread only.
  int _idleFrames = 0;
};

std::shared_ptr<AnimationFrameSource> makeAppleAnimationFrameSource() {
  return AppleAnimationFrameSource::shared();
}

#else

// macOS: none for now. Each runtime keeps its own requestAnimationFrame.
class AppleAnimationFrameSource : public AnimationFrameSource {
public:
  void requestFrame() override {}
  void onFrame(CADisplayLink *) {}
};

std::shared_ptr<AnimationFrameSource> makeAppleAnimationFrameSource() {
  return nullptr;
}

#endif

} // namespace rnwgpu

@implementation RNWGAnimationFrameTarget {
  rnwgpu::AppleAnimationFrameSource *_source;
}

- (instancetype)initWithSource:(rnwgpu::AppleAnimationFrameSource *)source {
  if ((self = [super init])) {
    _source = source;
  }
  return self;
}

- (void)onFrame:(CADisplayLink *)link {
  _source->onFrame(link);
}

@end
