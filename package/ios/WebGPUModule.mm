#import "WebGPUModule.h"
#import <React/RCTLog.h>

@implementation WebGPUModule

// To export a module named WebGPUModule
RCT_EXPORT_MODULE(WebGPUModule)

// Example method to be called from JavaScript
RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  RCTLogInfo(@"Log from WebGPUModule");
  return @true;
}

@end
