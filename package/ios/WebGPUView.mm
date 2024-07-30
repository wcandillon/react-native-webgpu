#ifdef RCT_NEW_ARCH_ENABLED
#import "WebGPUView.h"

#import <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#import <react/renderer/components/RNWgpuViewSpec/Props.h>
#import <react/renderer/components/RNWgpuViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import "Utils.h"
#import "MetalView.h"
#import "WebGPUModule.h"
#import "WebGPUViewComponentDescriptor.h"

using namespace facebook::react;

@interface WebGPUView () <RCTWebGPUViewViewProtocol>

@end

@implementation WebGPUView

static NSMutableDictionary<NSNumber *, MetalView *> *metalViewRegistry = [NSMutableDictionary new];

+ (ComponentDescriptorProvider)componentDescriptorProvider {
  return concreteComponentDescriptorProvider<WebGPUViewComponentDescriptor>();
}

+ (void)registerMetalView:(MetalView *)metalView withContextId:(NSNumber *)contextId
{
  metalViewRegistry[contextId] = metalView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const WebGPUViewProps>();
    _props = defaultProps;
  }

  return self;
}

- (void)prepareForRecycle
{
  [super prepareForRecycle];
  self.contentView = nil;
}

- (void)updateLayoutMetrics:(const facebook::react::LayoutMetrics &)layoutMetrics
           oldLayoutMetrics:(const facebook::react::LayoutMetrics &)oldLayoutMetrics
{
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  if (!self.contentView) {
    const auto &props = *std::static_pointer_cast<WebGPUViewProps const>(_props);
    auto contextId = @(props.contextId);
    self.contentView = metalViewRegistry[contextId];
    [metalViewRegistry removeObjectForKey:contextId];
  }
}

Class<RCTComponentViewProtocol> WebGPUViewCls(void) { return WebGPUView.class; }

@end
#endif
