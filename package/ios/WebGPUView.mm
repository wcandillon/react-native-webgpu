#ifdef RCT_NEW_ARCH_ENABLED
#import "WebGPUView.h"

#import <react/renderer/components/RNWgpuViewSpec/ComponentDescriptors.h>
#import <react/renderer/components/RNWgpuViewSpec/EventEmitters.h>
#import <react/renderer/components/RNWgpuViewSpec/Props.h>
#import <react/renderer/components/RNWgpuViewSpec/RCTComponentViewHelpers.h>

#import "RCTFabricComponentsPlugins.h"
#import "Utils.h"
#import "MetalView.h"
#import "WebGPUModule.h"

using namespace facebook::react;

@interface WebGPUView () <RCTWebGPUViewViewProtocol>

@end

@implementation WebGPUView {
  UIView *_view;
  BOOL _isConfigured;
}

+ (ComponentDescriptorProvider)componentDescriptorProvider {
  return concreteComponentDescriptorProvider<WebGPUViewComponentDescriptor>();
}

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    static const auto defaultProps = std::make_shared<const WebGPUViewProps>();
    _props = defaultProps;
    _view = [MetalView new];
    self.contentView = _view;
  }

  return self;
}

- (void)updateLayoutMetrics:(const facebook::react::LayoutMetrics &)layoutMetrics oldLayoutMetrics:(const facebook::react::LayoutMetrics &)oldLayoutMetrics
{
  [super updateLayoutMetrics:layoutMetrics oldLayoutMetrics:oldLayoutMetrics];
  
  if (_isConfigured) {
    return;
  }
  _isConfigured = YES;
  
  const auto &viewProps = *std::static_pointer_cast<WebGPUViewProps const>(_props);
  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = (void *)CFBridgingRetain(self.layer);
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  rnwgpu::RNWebGPUManager *manager = [WebGPUModule getManager];
  auto surfaceGpu = std::make_shared<wgpu::Surface>(manager->getGPU()->get().CreateSurface(&surfaceDescriptor));
  float width = _view.frame.size.width;
  float height = _view.frame.size.height;
  rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
  manager->surfacesRegistry.addSurface(viewProps.contextId, surfaceData);
  [WebGPUModule onSurfaceCreated:@(viewProps.contextId)];
}

Class<RCTComponentViewProtocol> WebGPUViewCls(void) { return WebGPUView.class; }

@end
#endif
