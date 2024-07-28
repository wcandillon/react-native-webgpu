#import "RCTBridge.h"
#import "WebGPUModule.h"
#import "webgpu_cpp.h"
#import <React/RCTUIManager.h>
#import <React/RCTViewManager.h>

@interface MetalView : UIView
@end

@implementation MetalView {
  __weak WebGPUModule *_webGPUModule;
  NSNumber *_contextId;
  BOOL _isConfigured;
}

+ (Class)layerClass {
  return [CAMetalLayer class];
}

- (void)setContectId:(NSNumber *)contextId
        webGPUModule:(WebGPUModule *)webGPUModule {
  _contextId = contextId;
  _webGPUModule = webGPUModule;
}

- (void)reactSetFrame:(CGRect)frame {
  [super reactSetFrame:frame];
  if (_isConfigured) {
    return;
  }
  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = (void *)CFBridgingRetain(self.layer);
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  rnwgpu::RNWebGPUManager *manager = [_webGPUModule getManager];
  auto surfaceGpu = std::make_shared<wgpu::Surface>(
      manager->getGPU()->get().CreateSurface(&surfaceDescriptor));
  CGFloat scaleFactor = [UIScreen mainScreen].scale;
  float width = self.frame.size.width * scaleFactor;
  float height = self.frame.size.height * scaleFactor;
  rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
  [_webGPUModule getManager]->surfacesRegistry.addSurface([_contextId intValue],
                                                          surfaceData);
  _isConfigured = YES;
}

@end

@interface WebGPUViewManager : RCTViewManager
@end

@implementation WebGPUViewManager

RCT_EXPORT_MODULE(WebGPUView)

- (UIView *)view {
  return [MetalView new];
}

RCT_CUSTOM_VIEW_PROPERTY(contextId, NSNumber, UIView) {
  WebGPUModule *webGPUModule =
      [self.bridge moduleForClass:[WebGPUModule class]];
  NSNumber *contextId = [RCTConvert NSNumber:json];
  [(MetalView *)view setContectId:contextId webGPUModule:webGPUModule];
}

@end
