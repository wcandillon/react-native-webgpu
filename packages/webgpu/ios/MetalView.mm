#import "MetalView.h"
#import "SurfaceUtils.h"
#import "webgpu_cpp.h"

#ifndef RCT_NEW_ARCH_ENABLED
#import <React/RCTViewManager.h>
#endif //RCT_NEW_ARCH_ENABLED

@implementation MetalView {
  BOOL _isConfigured;
}

+ (Class)layerClass {
  return [CAMetalLayer class];
}

- (void)configure
{
  [SurfaceUtils configureSurface:self.layer size:self.frame.size contextId:[_contextId intValue]];
}

- (void)update
{
  [SurfaceUtils updateSurface:[_contextId intValue] size:self.frame.size];
}

- (void)dealloc
{
  [SurfaceUtils cleanupSurface:[_contextId intValue]];
}

#ifndef RCT_NEW_ARCH_ENABLED
// Paper only method
// TODO: this method is wrong, it appears to call configureSurface instead of
// updateSurface sometimes
- (void)reactSetFrame:(CGRect)frame {
  [super reactSetFrame:frame];
  if (!_isConfigured) {
    [self configure];
    _isConfigured = YES;
  } else {
    [self update];
  }
}
#endif //RCT_NEW_ARCH_ENABLED

@end
