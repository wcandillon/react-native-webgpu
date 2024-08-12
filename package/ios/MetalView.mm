#import "MetalView.h"
#import "SurfaceUtils.h"
#import "webgpu_cpp.h"
#import <React/RCTViewManager.h>

@implementation MetalView {
  BOOL _isConfigured;
}

+ (Class)layerClass {
  return [CAMetalLayer class];
}

// Paper only method
- (void)reactSetFrame:(CGRect)frame {
  [super reactSetFrame:frame];
  if (!_isConfigured) {
    [SurfaceUtils configureSurface:self.layer
                              size:self.frame.size
                         contextId:[_contextId intValue]];
    _isConfigured = YES;
  } else {
    [SurfaceUtils updateSurface:[_contextId intValue] size:self.frame.size];
  }
}

@end
