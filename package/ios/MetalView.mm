#import "MetalView.h"
#import "webgpu_cpp.h"

@implementation MetalView {
  __weak WebGPUModule *_webGPUModule;
  NSNumber *_contextId;
  BOOL _isConfigured;
}

+ (Class)layerClass {
  return [CAMetalLayer class];
}

- (void)setContectId:(NSNumber *)contextId webGPUModule:(WebGPUModule *)webGPUModule
{
  _contextId = contextId;
  _webGPUModule = webGPUModule;
}

- (void)reactSetFrame:(CGRect)frame
{
//  [super reactSetFrame:frame];
  if (_isConfigured) {
    return;
  }
//  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
//  metalSurfaceDesc.layer = (void *)CFBridgingRetain(self.layer);
//  wgpu::SurfaceDescriptor surfaceDescriptor;
//  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
//  rnwgpu::RNWebGPUManager *manager = [_webGPUModule getManager];
//  auto surfaceGpu = std::make_shared<wgpu::Surface>(manager->getGPU()->get().CreateSurface(&surfaceDescriptor));
//  float width = self.frame.size.width;
//  float height = self.frame.size.height;
//  rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
//  [_webGPUModule getManager]->surfacesRegistry.addSurface([_contextId intValue], surfaceData);
  _isConfigured = YES;
}

@end
