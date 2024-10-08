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

- (void)willMoveToSuperview:(UIView *)newSuperview {
    [super willMoveToSuperview:newSuperview];

    if (newSuperview == nil) {
        // The view is being removed from its superview
        // Add your cleanup code here
        [SurfaceUtils cleanupSurface:[_contextId intValue]];
        _isConfigured = NO;
    }
}

@end
