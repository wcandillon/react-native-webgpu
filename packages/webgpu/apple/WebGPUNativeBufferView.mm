#import "WebGPUNativeBufferView.h"

#import <QuartzCore/QuartzCore.h>

#import <react/renderer/components/RNWgpuViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#import <react/renderer/components/RNWgpuViewSpec/Props.h>
#import <react/renderer/components/RNWgpuViewSpec/RCTComponentViewHelpers.h>

#import <React/RCTFabricComponentsPlugins.h>

#include <cmath>
#include <memory>

#include "NativeBufferViewRegistry.h"

using namespace facebook::react;

static const std::shared_ptr<const WebGPUNativeBufferViewProps> &
defaultNativeBufferViewProps() {
  static const auto props =
      std::make_shared<const WebGPUNativeBufferViewProps>();
  return props;
}

static NSString *gravityForResizeMode(WebGPUNativeBufferViewResizeMode mode) {
  switch (mode) {
  case WebGPUNativeBufferViewResizeMode::Contain:
    return kCAGravityResizeAspect;
  case WebGPUNativeBufferViewResizeMode::Cover:
    return kCAGravityResizeAspectFill;
  case WebGPUNativeBufferViewResizeMode::TopLeft:
#if !TARGET_OS_OSX
    // The gravity constants assume a bottom-left origin; UIKit layers have
    // a top-left one, so "bottom" is the top edge on screen.
    return kCAGravityBottomLeft;
#else
    return kCAGravityTopLeft;
#endif
  case WebGPUNativeBufferViewResizeMode::Stretch:
  default:
    return kCAGravityResize;
  }
}

@implementation WebGPUNativeBufferView {
  // Shows the IOSurface as its layer's contents. A subview rather than this
  // view's own layer, which keeps React Native's background and borders.
  RNWGPlatformView *_surfaceView;
  int _viewId;
  // The registry sequence number of what is shown, 0 for nothing.
  uint64_t _sequence;
  int _reportedWidth;
  int _reportedHeight;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider {
  return concreteComponentDescriptorProvider<
      WebGPUNativeBufferViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    // updateProps: casts _props to WebGPUNativeBufferViewProps, so it must
    // hold that type from the start (see WebGPUView).
    _props = defaultNativeBufferViewProps();
    _surfaceView = [RNWGPlatformView new];
#if TARGET_OS_OSX
    _surfaceView.wantsLayer = YES;
#else
    _surfaceView.userInteractionEnabled = NO;
#endif
    CALayer *layer = _surfaceView.layer;
    layer.contentsGravity = kCAGravityResize;
    layer.masksToBounds = YES;
    self.contentView = _surfaceView;
  }
  return self;
}

- (void)dealloc {
  if (_viewId != 0) {
    rnwgpu::NativeBufferViewRegistry::getInstance().detachView(_viewId);
  }
}

- (void)prepareForRecycle {
  [super prepareForRecycle];
  [self attachToViewId:0];
  [self setSurfaceContents:nil];
  _reportedWidth = 0;
  _reportedHeight = 0;
  // Reset to the default props (viewId == 0) so the next mount attaches.
  _props = defaultNativeBufferViewProps();
}

- (void)updateProps:(const Props::Shared &)props
           oldProps:(const Props::Shared &)oldProps {
  const auto &oldViewProps =
      *std::static_pointer_cast<const WebGPUNativeBufferViewProps>(_props);
  const auto &newViewProps =
      *std::static_pointer_cast<const WebGPUNativeBufferViewProps>(props);

  if (newViewProps.viewId != oldViewProps.viewId) {
    [self attachToViewId:newViewProps.viewId];
  }
  NSString *gravity = gravityForResizeMode(newViewProps.resizeMode);
  if (![_surfaceView.layer.contentsGravity isEqualToString:gravity]) {
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    _surfaceView.layer.contentsGravity = gravity;
    [CATransaction commit];
  }

  [super updateProps:props oldProps:oldProps];
}

- (void)updateEventEmitter:(const EventEmitter::Shared &)eventEmitter {
  [super updateEventEmitter:eventEmitter];
  [self reportPixelSize];
}

- (void)updateLayoutMetrics:(const LayoutMetrics &)layoutMetrics
           oldLayoutMetrics:(const LayoutMetrics &)oldLayoutMetrics {
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  // With the screen scale, a buffer of the reported size maps 1:1 to pixels,
  // and topLeft shows it unscaled.
  _surfaceView.layer.contentsScale = layoutMetrics.pointScaleFactor;
  [self reportPixelSize];
}

// onPixelSizeChange: the size the producer should render at for a 1:1
// display. Layout frames are already on the pixel grid; the rounding only
// absorbs floating point error, the same rounding as Android's.
- (void)reportPixelSize {
  const auto scale = _layoutMetrics.pointScaleFactor;
  const auto size = _layoutMetrics.frame.size;
  const int width = static_cast<int>(std::lround(size.width * scale));
  const int height = static_cast<int>(std::lround(size.height * scale));
  if (width <= 0 || height <= 0 || _eventEmitter == nullptr ||
      (width == _reportedWidth && height == _reportedHeight)) {
    return;
  }
  _reportedWidth = width;
  _reportedHeight = height;
  std::static_pointer_cast<const WebGPUNativeBufferViewEventEmitter>(
      _eventEmitter)
      ->onPixelSizeChange({.width = width, .height = height});
}

// Main thread.
- (void)attachToViewId:(int)viewId {
  auto &registry = rnwgpu::NativeBufferViewRegistry::getInstance();
  if (_viewId != 0) {
    registry.detachView(_viewId);
  }
  _viewId = viewId;
  _sequence = 0;
  if (viewId == 0) {
    return;
  }
  // Called from any thread (the fence listener's queue, the JS thread)
  // when content is ready, and right away when there is some already.
  __weak WebGPUNativeBufferView *weakSelf = self;
  registry.setListener(viewId, [weakSelf]() {
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf consumeContent];
    });
  });
}

// Main thread: shows the newest content, if it is newer than what is shown.
// Present on change, no frame pacing: Core Animation shows it on the next
// vsync, and retains the IOSurface for as long as it does.
- (void)consumeContent {
  if (_viewId == 0) {
    return;
  }
  uint64_t sequence = 0;
  auto content = rnwgpu::NativeBufferViewRegistry::getInstance().content(
      _viewId, &sequence);
  if (content == nullptr || sequence <= _sequence) {
    return;
  }
  _sequence = sequence;
  [self setSurfaceContents:(__bridge id)content->buffer()];
}

- (void)setSurfaceContents:(nullable id)contents {
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  _surfaceView.layer.contents = contents;
  [CATransaction commit];
}

Class<RCTComponentViewProtocol> WebGPUNativeBufferViewCls(void) {
  return WebGPUNativeBufferView.class;
}

@end
