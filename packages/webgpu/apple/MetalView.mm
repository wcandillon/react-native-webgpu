#import "MetalView.h"
#import "SurfaceUtils.h"
#import "webgpu_cpp.h"
#import <React/RCTViewManager.h>

@implementation MetalView {
  BOOL _isConfigured;
}

#if !TARGET_OS_OSX
+ (Class)layerClass {
  return [CAMetalLayer class];
}
#else // !TARGET_OS_OSX
- (instancetype)init
{
    self = [super init];
    if (self) {
        self.wantsLayer = true;
        self.layer = [CAMetalLayer layer];
    }
    return self;
}
#endif // !TARGET_OS_OSX

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
