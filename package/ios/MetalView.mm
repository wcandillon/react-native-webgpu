#import <React/RCTViewManager.h>
#import "MetalView.h"
#import "SurfaceUtils.h"
#import "webgpu_cpp.h"

@implementation MetalView {
  BOOL _isConfigured;
}

+ (Class)layerClass {
  return [CAMetalLayer class];
}

// Paper only method
- (void)reactSetFrame:(CGRect)frame
{
  [super reactSetFrame:frame];
  if (!_isConfigured) {
    [SurfaceUtils configureSurface:self.layer size:self.frame.size contextId:[_contextId intValue]];
    _isConfigured = YES;
  }
}

@end
