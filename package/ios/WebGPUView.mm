#ifdef RCT_NEW_ARCH_ENABLED
#import "WebGPUView.h"

#import <react/renderer/components/RNWebGPUViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/RNWebGPUViewSpec/EventEmitters.h>
#import <react/renderer/components/RNWebGPUViewSpec/Props.h>
#import <react/renderer/components/RNWebGPUViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import "Utils.h"

using namespace facebook::react;

@interface WebGPUView () <RCTWebGPUViewViewProtocol>

@end

@implementation WebGPUView {
  UIView *_view;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider {
  return concreteComponentDescriptorProvider<WgpuViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const WgpuViewProps>();
    _props = defaultProps;

    _view = [[UIView alloc] init];

    self.contentView = _view;
  }

  return self;
}

- (void)updateProps:(Props::Shared const &)props
           oldProps:(Props::Shared const &)oldProps {
  const auto &oldViewProps =
      *std::static_pointer_cast<WebGPUViewProps const>(_props);
  const auto &newViewProps =
      *std::static_pointer_cast<WebGPUViewProps const>(props);

  if (oldViewProps.color != newViewProps.color) {
    NSString *colorToConvert =
        [[NSString alloc] initWithUTF8String:newViewProps.color.c_str()];
    [_view setBackgroundColor:[Utils hexStringToColor:colorToConvert]];
  }

  [super updateProps:props oldProps:oldProps];
}

Class<RCTComponentViewProtocol> WebGPUViewCls(void) { return WebGPUView.class; }

@end
#endif
