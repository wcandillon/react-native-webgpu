#import "MetalView.h"
#import "RCTBridge.h"
#import "RNWGUIKit.h"
#import "WebGPUModule.h"
#import <React/RCTUIManager.h>
#import <React/RCTViewManager.h>

@interface WebGPUViewManager : RCTViewManager
@end

@implementation WebGPUViewManager

RCT_EXPORT_MODULE(WebGPUView)

- (RNWGPlatformView *)view {
  return [MetalView new];
}

RCT_CUSTOM_VIEW_PROPERTY(contextId, NSNumber, RNWGPlatformView) {
  NSNumber *contextId = [RCTConvert NSNumber:json];
  [(MetalView *)view setContextId:contextId];
}

@end
