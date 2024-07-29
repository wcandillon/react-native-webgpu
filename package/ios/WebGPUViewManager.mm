#import "RCTBridge.h"
#import <React/RCTUIManager.h>
#import <React/RCTViewManager.h>
#import "WebGPUModule.h"
#import "MetalView.h"

@interface WebGPUViewManager : RCTViewManager
@end

@implementation WebGPUViewManager

RCT_EXPORT_MODULE(WebGPUView)

- (UIView *)view {
  return [MetalView new];
}

RCT_CUSTOM_VIEW_PROPERTY(contextId, NSNumber, UIView) {
  WebGPUModule *webGPUModule = [self.bridge moduleForClass:[WebGPUModule class]];
  NSNumber *contextId = [RCTConvert NSNumber:json];
  [(MetalView *)view setContectId:contextId webGPUModule:webGPUModule];
}

@end
