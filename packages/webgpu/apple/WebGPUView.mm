#import "WebGPUView.h"

#import <react/renderer/components/RNWgpuViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#import <react/renderer/components/RNWgpuViewSpec/Props.h>
#import <react/renderer/components/RNWgpuViewSpec/RCTComponentViewHelpers.h>

#import "MetalView.h"
#import <React/RCTFabricComponentsPlugins.h>

using namespace facebook::react;

static const std::shared_ptr<const WebGPUViewProps> &defaultWebGPUViewProps() {
  static const auto props = std::make_shared<const WebGPUViewProps>();
  return props;
}

@implementation WebGPUView

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
  }
  return self;
}

- (void)prepareForRecycle {
  [super prepareForRecycle];
  /*
    It's important to destroy the Metal Layer before releasing a view
    to the recycled pool to prevent displaying outdated content from
    the last usage in the new context.
  */
  self.contentView = nil;
  /*
    Reset to the default props (contextId == 0) so the next mount always
    detects a contextId change and configures a fresh Metal layer.
  */
  _props = defaultWebGPUViewProps();
}

- (MetalView *)getContentView {
  if (!self.contentView) {
    self.contentView = [MetalView new];
  }
  return (MetalView *)self.contentView;
}

- (void)updateProps:(const Props::Shared &)props
           oldProps:(const Props::Shared &)oldProps {
  const auto &oldViewProps =
      *std::static_pointer_cast<const WebGPUViewProps>(_props);
  const auto &newViewProps =
      *std::static_pointer_cast<const WebGPUViewProps>(props);

  if (newViewProps.contextId != oldViewProps.contextId) {
    /*
      The context is set only once during mounting the component
      and never changes because it isn't available for users to modify.
    */
    MetalView *metalView = [MetalView new];
    self.contentView = metalView;
    [metalView setContextId:@(newViewProps.contextId)];
    [metalView configure];
  }

  [super updateProps:props oldProps:oldProps];
}

- (void)updateLayoutMetrics:(const LayoutMetrics &)layoutMetrics
           oldLayoutMetrics:(const LayoutMetrics &)oldLayoutMetrics {
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  [(MetalView *)self.contentView update];
}

Class<RCTComponentViewProtocol> WebGPUViewCls(void) { return WebGPUView.class; }

@end
