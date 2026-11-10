#import "WebGPUView.h"

#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>

#import <React/RCTFabricComponentsPlugins.h>

#import <react/renderer/components/RNWgpuViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#import <react/renderer/components/RNWgpuViewSpec/Props.h>
#import <react/renderer/components/RNWgpuViewSpec/RCTComponentViewHelpers.h>

#include <algorithm>
#include <atomic>
#include <memory>

#import "RNWGPresentThread.h"
#include "SurfaceRegistry.h"
#include "WebGPURuntime.h"

using namespace facebook::react;

// React Native 0.82 added invalidate to RCTComponentViewProtocol; before that
// nothing declares or implements it. See -[WebGPUView invalidate].
@interface RCTViewComponentView (WebGPUInvalidate)
- (void)invalidate;
@end

@interface WebGPUView ()
// What the frame scheduler asks of the view, on the present thread (see
// SurfaceInfo::setPostFrameCallback).
- (void)postFrameCallback;
@end

static const std::shared_ptr<const WebGPUViewProps> &defaultWebGPUViewProps() {
  static const auto props = std::make_shared<const WebGPUViewProps>();
  return props;
}

// Whether the window can show a frame: not while the app is in the background
// (hidden on macOS), where GPU work is refused. Read once when the view is
// created; the application's notifications keep it up to date from then on
// (see appDidEnterBackground:). The application state is main-thread only, so
// this is answered there only.
static bool appIsBackgrounded() {
#if !TARGET_OS_OSX
  auto state = UIApplication.sharedApplication.applicationState;
  return state == UIApplicationStateBackground;
#else
  return NSApplication.sharedApplication.isHidden;
#endif // !TARGET_OS_OSX
}

// The view and its window on the present thread, as React Native Skia's
// SkiaView does it (see RNWGPresentThread and SurfaceInfo): the layer is
// attached and detached there, in order with the presents, and a composited
// canvas's frames are presented into it there, on the display link of that
// thread (a direct canvas's render loop presents them itself, see
// SurfaceInfo). The main thread only lays the layer out, posts, and never
// waits.
@implementation WebGPUView {
  // The canvas, from the contextId prop until the view is recycled, and the
  // layer its frames are presented into, while the view is in a superview
  // (see willMoveToSuperview:).
  std::shared_ptr<rnwgpu::SurfaceInfo> _info;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  CAMetalLayer *_layer;
#pragma clang diagnostic pop
  int _contextId;
  // Whether the window can show a frame (see appIsBackgrounded). Written on
  // the main thread, read on the present thread, by the canvas.
  std::shared_ptr<std::atomic<bool>> _canPresent;
  // Whether the present thread presents the view: from the enter task to the
  // leave task. Present thread.
  std::shared_ptr<bool> _presenting;
  // The main thread's record of the window: whether one is attached (the
  // attach itself runs on the present thread, see attachWindow).
  bool _windowAttached;
#if !TARGET_OS_OSX
  // Present thread.
  CADisplayLink *_displayLink;
  // Whether the display link asked for the display's full rate yet (see
  // requestFullFrameRate). Present thread.
  bool _fullFrameRateRequested;
  // The display's rate, read on the main thread as the view enters a window
  // (UIKit), 0 until then.
  std::atomic<float> _maximumFramesPerSecond;
#else
  // Whether a tick is scheduled, when the last one ran (see
  // presentAgainLater), and which ticks are still wanted: the leave moves
  // the count on, and a tick scheduled before it does nothing. Present
  // thread.
  bool _presentAgainScheduled;
  CFTimeInterval _lastTick;
  uint64_t _tickGeneration;
#endif
}

+ (ComponentDescriptorProvider)componentDescriptorProvider {
  return concreteComponentDescriptorProvider<WebGPUViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    /*
      The base class initializes _props with a plain ViewProps instance;
      updateProps: casts _props to WebGPUViewProps, so it must hold our
      concrete props type from the start (RN 0.87+ asserts on this).
    */
    _props = defaultWebGPUViewProps();
    _contextId = 0;
    _canPresent = std::make_shared<std::atomic<bool>>(!appIsBackgrounded());
    _presenting = std::make_shared<bool>(false);
    _windowAttached = false;
#if TARGET_OS_OSX
    _presentAgainScheduled = false;
    _lastTick = 0;
    _tickGeneration = 0;
#else
    _fullFrameRateRequested = false;
    _maximumFramesPerSecond = 0;
#endif
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if !TARGET_OS_OSX
    [center addObserver:self
               selector:@selector(appDidEnterBackground:)
                   name:UIApplicationDidEnterBackgroundNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(appWillEnterForeground:)
                   name:UIApplicationWillEnterForegroundNotification
                 object:nil];
#else
    [center addObserver:self
               selector:@selector(appDidEnterBackground:)
                   name:NSApplicationDidHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(appWillEnterForeground:)
                   name:NSApplicationDidUnhideNotification
                 object:nil];
#endif
  }
  return self;
}

#pragma mark - Lifecycle

// The layer is made when the view gets a superview and goes with it, its
// drawables included, so that a view out of the hierarchy holds none; the
// canvas keeps its frames, and the next layer shows the last of them (see
// SurfaceInfo::attach).
#if !TARGET_OS_OSX
- (void)willMoveToSuperview:(UIView *)newSuperView {
  [super willMoveToSuperview:newSuperView];
#else
- (void)viewWillMoveToSuperview:(NSView *)newSuperView {
  [super viewWillMoveToSuperview:newSuperView];
#endif // !TARGET_OS_OSX
  if (newSuperView == nullptr || _layer != nil) {
    return;
  }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  _layer = [CAMetalLayer layer];
#pragma clang diagnostic pop
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _layer.frame =
      CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height);
  [self.layer addSublayer:_layer];
  [CATransaction commit];
  // AppKit lays out a view put in a superview only when asked.
  [self setNeedsLayout];
  if (_info != nullptr) {
    [self enterPresentThread];
    [self attachWindow];
  }
}

- (void)removeFromSuperview {
  [self leavePresentThread];
  [_layer removeFromSuperlayer];
  _layer = nil;
  [super removeFromSuperview];
}

// Main thread, as the view gets a superview with a canvas: the present thread
// presents the view from the tasks posted after this one on. The entries of
// the canvas that belong to the present thread are bound to it there, ahead
// of everything posted behind this.
- (void)enterPresentThread {
  __weak WebGPUView *weakSelf = self;
  auto info = _info;
  auto presenting = _presenting;
  auto canPresent = _canPresent;
  [RNWGPresentThread post:^{
    *presenting = true;
    info->bindPresentThread();
    // A frame is waiting (see SurfaceInfo::present): the scheduler arms the
    // display link for it.
    info->setPostFrameCallback([weakSelf]() { [weakSelf postFrameCallback]; });
    // The window is left alone while the app is in the background: the frame
    // is presented once the app is back (see appWillEnterForeground:).
    info->setCanPresent([canPresent]() { return canPresent->load(); });
  }];
}

// Main thread, as the view leaves its superview or is recycled: the present
// thread stops presenting the view, and lets go of the window, after the
// present in flight if any, so that the main thread does not wait for it.
// Only this layer's window: the view may be back with a new one by then. The
// task holds the layer, which so outlives the Dawn surface over it, and whose
// address cannot come back as the new layer's meanwhile. The display link
// goes in the same task: a frame scheduled between two would make a new one
// that outlives the leave (see postFrameCallback). The view goes on the main
// thread, where UIKit expects it to, as this task owns the last reference to
// it once Fabric let go of it (see invalidate). A strong capture cannot
// promise that: the block would let go on the present thread, racing the main
// thread for the last release. So the reference is taken here, by hand, and
// let go of only on the main thread, once the autorelease pool of the work is
// drained.
- (void)leavePresentThread {
  if (_info == nullptr) {
    return;
  }
  auto info = _info;
  CAMetalLayer *layer = _layer;
  void *retainedSelf = (__bridge_retained void *)self;
  [RNWGPresentThread post:^{
    @autoreleasepool {
      __unsafe_unretained WebGPUView *view =
          (__bridge WebGPUView *)retainedSelf;
      *view->_presenting = false;
      // Whatever the scheduler asked for goes with the link.
      info->cancelScheduledFrames();
      info->setPostFrameCallback(nullptr);
#if !TARGET_OS_OSX
      [view->_displayLink invalidate];
      view->_displayLink = nil;
      // The request belonged to that link: the next one must ask again.
      view->_fullFrameRateRequested = false;
#else
      // A tick scheduled is let go of (see presentAgainLater): the scheduler
      // forgot it, and the next request schedules one of its own.
      view->_presentAgainScheduled = false;
      view->_tickGeneration++;
#endif
      info->detach((__bridge void *)layer);
    }
    dispatch_async_f(dispatch_get_main_queue(), retainedSelf,
                     [](void *retained) { CFRelease(retained); });
  }];
  _windowAttached = false;
}

// Main thread: the display link held the view until the leave task
// invalidated it, and that task handed the view back here (see
// leavePresentThread). A view that got here with its canvas (neither
// prepareForRecycle nor invalidate came, on a React Native before 0.82 for a
// view Fabric does not recycle) retires it without a reference to itself,
// which a deallocating object cannot hand out: the window goes with the
// canvas, on the present thread.
- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  if (_info == nullptr) {
    return;
  }
  auto info = _info;
  CAMetalLayer *layer = _layer;
  [RNWGPresentThread post:^{
    info->cancelScheduledFrames();
    info->setPostFrameCallback(nullptr);
    info->detach((__bridge void *)layer);
  }];
  _info->setViewAttached(false);
  rnwgpu::SurfaceRegistry::getInstance().removeSurfaceInfo(_contextId);
}

// Fabric either recycles an unmounted view (prepareForRecycle) or, with the
// pool full or the view opting out, lets go of it (invalidate): the canvas
// and the window go now either way, rather than with the view.
- (void)prepareForRecycle {
  [super prepareForRecycle];
  [self releaseCanvas];
  /*
    Reset to the default props (contextId == 0) so the next mount always
    detects a contextId change and takes a fresh canvas.
  */
  _props = defaultWebGPUViewProps();
}

// Only called from React Native 0.82 on, which implements it on every view:
// on an earlier version nothing gets here. Drop the category above once the
// peer range starts at 0.82.
- (void)invalidate {
  [super invalidate];
  [self releaseCanvas];
}

// The view dies with its Canvas (contextIds are never reused), so view
// teardown retires the registry entry. The JS-side cleanup
// (RNWebGPU.destroyContext) only handles entries that never had a view; see
// RNWebGPU::destroyContext for the ownership split. The window goes on the
// present thread, behind the present in flight.
- (void)releaseCanvas {
  if (_info == nullptr) {
    return;
  }
  [self leavePresentThread];
  [_layer removeFromSuperlayer];
  _layer = nil;
  _info->setViewAttached(false);
  rnwgpu::SurfaceRegistry::getInstance().removeSurfaceInfo(_contextId);
  _info = nullptr;
  _contextId = 0;
}

#pragma mark - Application state

// The app went to the background (was hidden on macOS): frames wait there.
- (void)appDidEnterBackground:(NSNotification *)notification {
  _canPresent->store(false);
}

// The app is coming back: what waited is presented on the view's next frame.
- (void)appWillEnterForeground:(NSNotification *)notification {
  _canPresent->store(true);
  if (_info != nullptr) {
    auto info = _info;
    auto presenting = _presenting;
    [RNWGPresentThread post:^{
      if (*presenting) {
        info->onCanPresent();
      }
    }];
  }
}

#pragma mark - Frames

// The scheduler asks for the next vsync: the display link, which ticks on
// the present thread, started and unpaused here and paused again once the
// scheduler has nothing outstanding (see onFrame:), so that an idle view
// does not keep the display at its full rate. macOS has no CADisplayLink on
// the minimum deployment target, so the frame is presented a frame later
// there (see presentAgainLater). Present thread.
- (void)postFrameCallback {
  if (!*_presenting) {
    // Out of its superview, the frames wait: the view back in one asks for a
    // frame (see SurfaceInfo::attach), and a link would hold the view.
    return;
  }
#if !TARGET_OS_OSX
  if (_displayLink == nil) {
    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                               selector:@selector(onFrame:)];
    [_displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  [self requestFullFrameRate];
  _displayLink.paused = NO;
#else
  [self presentAgainLater];
#endif
}

#if !TARGET_OS_OSX
// The display's rate, for the display link (see requestFullFrameRate): UIKit
// is only read on the main thread.
- (void)didMoveToWindow {
  [super didMoveToWindow];
#if !TARGET_OS_VISION
  UIScreen *screen = self.window.screen;
  if (screen != nil) {
    _maximumFramesPerSecond = (float)screen.maximumFramesPerSecond;
  }
#endif
}

// A display link fires at 60 Hz until it asks for more, also while the
// display already runs at 120 Hz for another link (Reanimated asks for 120):
// a 120 Hz producer would then be presented every other frame. Ask for the
// display's full rate once the view knows its screen. On iPhone this needs
// CADisableMinimumFrameDurationOnPhone in the app's Info.plist too, else iOS
// caps every link at 60 Hz. The link is paused while nothing is queued, so an
// idle view does not keep the display at its full rate. Present thread.
- (void)requestFullFrameRate {
#if !TARGET_OS_VISION
  if (_fullFrameRateRequested) {
    return;
  }
  const float maxFps = _maximumFramesPerSecond.load();
  if (maxFps <= 0) {
    return;
  }
  _fullFrameRateRequested = true;
  if (maxFps > 60) {
    _displayLink.preferredFrameRateRange =
        CAFrameRateRangeMake(60, maxFps, maxFps);
  }
#endif
}

// No autorelease pool of its own: the link is a run loop source, so the
// thread's run loop returns after each tick and drains its pool then (see
// RNWGPresentThread).
- (void)onFrame:(CADisplayLink *)link {
  if (_info == nullptr || !*_presenting) {
    link.paused = YES;
    return;
  }
  _info->onFrame();
  // The scheduler asked for another tick while a frame is left: the window
  // could not take one.
  if (!_info->isFrameCallbackPosted()) {
    link.paused = YES;
  }
}
#else
// A tick stands in for the vsync macOS has no display link for on the
// minimum deployment target. The first tick after an idle stretch runs as
// soon as the present thread gets to it: nothing waits for a cadence that has
// not started. The ticks after it come at 60 Hz from the previous one, for
// the frames left over (the window could not take one) and the frames that
// keep coming. Present thread.
- (void)presentAgainLater {
  if (_presentAgainScheduled) {
    return;
  }
  _presentAgainScheduled = true;
  const CFTimeInterval interval = 1.0 / 60;
  const CFTimeInterval delay =
      std::max(0.0, _lastTick + interval - CACurrentMediaTime());
  const uint64_t generation = _tickGeneration;
  __weak WebGPUView *weakSelf = self;
  auto presenting = _presenting;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               static_cast<int64_t>(delay * NSEC_PER_SEC)),
                 dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
                   [RNWGPresentThread post:^{
                     if (!*presenting) {
                       return;
                     }
                     WebGPUView *strongSelf = weakSelf;
                     if (strongSelf == nil ||
                         strongSelf->_tickGeneration != generation ||
                         strongSelf->_info == nullptr) {
                       // Left meanwhile (see leavePresentThread): the
                       // scheduler forgot what it asked for, and asks again
                       // for what comes next.
                       return;
                     }
                     strongSelf->_presentAgainScheduled = false;
                     strongSelf->_lastTick = CACurrentMediaTime();
                     strongSelf->_info->onFrame();
                   }];
                 });
}
#endif

#pragma mark - Layout

// The layer fills the view, and the canvas learns the view's size in points
// (clientWidth and clientHeight in JS); the drawing buffer is the app's
// (canvas.width and height), and the layer scales it to the view.
- (void)layoutSubviews {
  [super layoutSubviews];
  if (_layer != nil) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _layer.frame =
        CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height);
    [CATransaction commit];
  }
  [self updateLayoutSize];
}

- (void)updateLayoutSize {
  if (_info != nullptr) {
    _info->setLayoutSize(static_cast<int>(self.bounds.size.width),
                         static_cast<int>(self.bounds.size.height));
  }
}

// Configures the layer as the Dawn surface of the canvas, on the present
// thread, in order with the presents, detaches and frames posted before it:
// a present in flight holds the thread for as long as the window takes, and
// the main thread must not wait for that. The main thread records the window
// as attached right away. The layer is retained for as long as the canvas
// holds it: the detach, on the present thread, can outlive this view, e.g.
// across a dev reload where the registry is cleared before dealloc runs.
// Balanced by the releaser, which runs the release on the main thread, where
// CALayer teardown belongs.
- (void)attachWindow {
  if (_windowAttached || _layer == nil || _info == nullptr) {
    return;
  }
  _windowAttached = true;
  auto info = _info;
  auto presenting = _presenting;
  void *layer = (void *)CFBridgingRetain(_layer);
  [RNWGPresentThread post:^{
    if (!*presenting) {
      dispatch_async(dispatch_get_main_queue(), ^{
        CFBridgingRelease(layer);
      });
      return;
    }
    info->attach(
        layer,
        [](void *detached) {
          dispatch_async(dispatch_get_main_queue(), ^{
            CFBridgingRelease(detached);
          });
        },
        rnwgpu::SurfaceInfo::Window::kShowsEachFrame);
  }];
}

#pragma mark - Props

- (void)updateProps:(const Props::Shared &)props
           oldProps:(const Props::Shared &)oldProps {
  const auto &oldViewProps =
      *std::static_pointer_cast<const WebGPUViewProps>(_props);
  const auto &newViewProps =
      *std::static_pointer_cast<const WebGPUViewProps>(props);
  if (newViewProps.contextId != oldViewProps.contextId) {
    /*
      The context is set only once during mounting the component and never
      changes because it isn't available for users to modify. The presentation
      prop is read along with it (see SurfaceInfo::setPresentation); a change
      of it alone does not reach the canvas, and the JS side warns about one.
    */
    [self bindContextId:newViewProps.contextId
           presentation:newViewProps.presentation];
  }
  [super updateProps:props oldProps:oldProps];
}

// The canvas of the contextId prop, claimed for this view (see
// SurfaceRegistry::attachView), presenting as the presentation prop says: the
// frames presented from now on are its.
- (void)bindContextId:(int)contextId
         presentation:(WebGPUViewPresentation)presentation {
  [self releaseCanvas];
  if (contextId == 0) {
    return;
  }
  _contextId = contextId;
  _info = rnwgpu::SurfaceRegistry::getInstance().attachView(
      contextId, rnwgpu::WebGPURuntime::shared().instance(),
      static_cast<int>(self.bounds.size.width),
      static_cast<int>(self.bounds.size.height),
      presentation == WebGPUViewPresentation::Composited
          ? rnwgpu::SurfaceInfo::Presentation::kComposited
          : rnwgpu::SurfaceInfo::Presentation::kDirect);
  if (_layer == nil && self.superview != nil) {
    // Back in a superview before the props came: the layer the superview
    // move makes (see willMoveToSuperview:) is made now instead.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    _layer = [CAMetalLayer layer];
#pragma clang diagnostic pop
    _layer.frame =
        CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height);
    [self.layer addSublayer:_layer];
  }
  if (_layer != nil) {
    [self enterPresentThread];
    [self attachWindow];
  }
}

@end

Class<RCTComponentViewProtocol> WebGPUViewCls(void) { return WebGPUView.class; }
