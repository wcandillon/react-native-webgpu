#import <React/RCTUIManager.h>
#import <React/RCTViewManager.h>
#import "MetalView.h"
#import "RCTBridge.h"
#import "WebGPUModule.h"

@interface WebGPUViewManager : RCTViewManager
@end

@implementation WebGPUViewManager

RCT_EXPORT_MODULE(WebGPUView)

- (UIView *)view {
  return [MetalView new];
}

RCT_CUSTOM_VIEW_PROPERTY(contextId, NSNumber, UIView) {
  NSNumber *contextId = [RCTConvert NSNumber:json];
  [(MetalView *)view setContextId:contextId];
}

@end
