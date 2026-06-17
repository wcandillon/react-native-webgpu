#import "WebGPUModule.h"
#include "ApplePlatformContext.h"
#include "ElementCaptureRegistry.h"
#import "GPUCanvasContext.h"

#import <React/RCTBridge+Private.h>
#import <React/RCTCallInvoker.h>
#import <React/RCTLog.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>

#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#include <atomic>
#include <memory>

namespace jsi = facebook::jsi;
namespace react = facebook::react;

// Category to declare the runtime property on RCTBridge/RCTBridgeProxy.
// In Bridgeless mode, self.bridge is an RCTBridgeProxy which implements
// -(void *)runtime. In Legacy mode, self.bridge is the real RCTBridge
// (backed by RCTCxxBridge) which also implements it.
@interface RCTBridge (JSIRuntime)
- (void *)runtime;
@end

// Fabric sets each mounted component view's UIView.tag to its React tag
// (RCTComponentViewRegistry: `componentViewDescriptor.view.tag = tag`), so we
// can resolve tag -> view by walking the window hierarchy. This works in both
// bridge and bridgeless modes, unlike `self.bridge.surfacePresenter` (nil in
// bridgeless).
static UIView *RNWGPUFindViewWithTag(UIView *root, NSInteger tag) {
  if (root.tag == tag) {
    return root;
  }
  for (UIView *subview in root.subviews) {
    UIView *found = RNWGPUFindViewWithTag(subview, tag);
    if (found) {
      return found;
    }
  }
  return nil;
}

static UIView *RNWGPUResolveView(NSInteger tag) {
  for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
    if (![scene isKindOfClass:[UIWindowScene class]]) {
      continue;
    }
    for (UIWindow *window in ((UIWindowScene *)scene).windows) {
      UIView *found = RNWGPUFindViewWithTag(window, tag);
      if (found) {
        return found;
      }
    }
  }
  return nil;
}

@implementation WebGPUModule

RCT_EXPORT_MODULE(WebGPUModule)

static std::shared_ptr<rnwgpu::RNWebGPUManager> webgpuManager;

// Monotonic token handed to JS, exchanged for the captured IOSurface via
// RNWebGPU.consumeCapturedElement(token).
static std::atomic<int> sCaptureToken{0};

// Synthesize callInvoker so RCTTurboModuleManager injects the JS CallInvoker.
// When the module conforms to RCTCallInvokerModule, the TurboModule infra
// calls setCallInvoker: during module initialization.
@synthesize callInvoker = _callInvoker;

+ (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return webgpuManager;
}

#pragma Setup and invalidation

+ (BOOL)requiresMainQueueSetup {
  return YES;
}

- (void)invalidate {
  webgpuManager = nil;
}

- (std::shared_ptr<rnwgpu::RNWebGPUManager>)getManager {
  return webgpuManager;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
  if (webgpuManager != nil) {
    // Already initialized, ignore call.
    return @true;
  }

  // self.bridge works in both Legacy (RCTBridge) and Bridgeless
  // (RCTBridgeProxy).
  jsi::Runtime *runtime = (jsi::Runtime *)self.bridge.runtime;
  if (!runtime) {
    NSLog(@"Failed to install react-native-webgpu: jsi::Runtime* was null! "
          @"(self.bridge=%@)",
          self.bridge);
    return [NSNumber numberWithBool:NO];
  }

  // _callInvoker is injected by RCTTurboModuleManager because we conform to
  // RCTCallInvokerModule. Works in both Legacy and Bridgeless.
  std::shared_ptr<react::CallInvoker> jsInvoker = _callInvoker.callInvoker;
  if (!jsInvoker) {
    NSLog(@"Failed to install react-native-webgpu: react::CallInvoker was "
          @"null!");
    return [NSNumber numberWithBool:NO];
  }

  std::shared_ptr<rnwgpu::PlatformContext> platformContext =
      std::make_shared<rnwgpu::ApplePlatformContext>();
  webgpuManager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsInvoker,
                                                            platformContext);
  return @true;
}

// "HTML in Canvas": render the native view with the given React tag off-screen
// into an IOSurface, then resolve with a token JS exchanges for the surface via
// RNWebGPU.consumeCapturedElement(token).
//
// v1 spike: a CPU CALayer.renderInContext into an IOSurface-backed CGContext.
// This reads the live on-screen layer without detaching it (so the view stays
// interactive) and sidesteps the CARenderer single-render-tree constraint. The
// GPU CARenderer path is the follow-up.
RCT_EXPORT_METHOD(captureElement
                  : (double)tag resolve
                  : (RCTPromiseResolveBlock)resolve reject
                  : (RCTPromiseRejectBlock)reject) {
  NSInteger reactTag = (NSInteger)tag;
  dispatch_async(dispatch_get_main_queue(), ^{
    @try {
      UIView *view = RNWGPUResolveView(reactTag);
      if (!view) {
        reject(@"E_NO_VIEW",
               [NSString stringWithFormat:@"No view found for tag %ld",
                                          (long)reactTag],
               nil);
        return;
      }
      CGFloat scale =
          view.window.screen.scale ?: UIScreen.mainScreen.scale ?: 1.0;
      CGSize ptSize = view.bounds.size;
      NSInteger width = (NSInteger)lround(ptSize.width * scale);
      NSInteger height = (NSInteger)lround(ptSize.height * scale);
      if (width <= 0 || height <= 0) {
        reject(@"E_EMPTY_VIEW", @"View has no size to capture yet", nil);
        return;
      }

      // BGRA8 IOSurface, matching the existing video path (Dawn imports
      // kCVPixelFormatType_32BGRA IOSurfaces as bgra8unorm).
      NSDictionary *props = @{
        (id)kIOSurfaceWidth : @(width),
        (id)kIOSurfaceHeight : @(height),
        (id)kIOSurfaceBytesPerElement : @(4),
        (id)kIOSurfacePixelFormat : @(kCVPixelFormatType_32BGRA),
      };
      IOSurfaceRef surface = IOSurfaceCreate((__bridge CFDictionaryRef)props);
      if (!surface) {
        reject(@"E_IOSURFACE", @"Failed to create IOSurface", nil);
        return;
      }

      IOSurfaceLock(surface, 0, NULL);
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
          IOSurfaceGetBaseAddress(surface), (size_t)width, (size_t)height, 8,
          IOSurfaceGetBytesPerRow(surface), cs,
          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
      CGColorSpaceRelease(cs);
      // Flip to UIKit's top-left origin and scale points -> pixels.
      CGContextTranslateCTM(ctx, 0, height);
      CGContextScaleCTM(ctx, scale, -scale);
      // drawViewHierarchyInRect snapshots the live render tree (including
      // editing UITextField/UITextView text); CALayer.renderInContext does not
      // capture text-input content, so it must not be used here.
      UIGraphicsPushContext(ctx);
      [view drawViewHierarchyInRect:CGRectMake(0, 0, ptSize.width, ptSize.height)
                 afterScreenUpdates:NO];
      UIGraphicsPopContext();
      CGContextRelease(ctx);
      IOSurfaceUnlock(surface, 0, NULL);

      rnwgpu::CapturedElementEntry entry;
      // IOSurfaceCreate returns a +1 reference; ownership transfers to the JS
      // consumer, which releases it via RNWebGPU.releaseCapturedElement once
      // the import has taken its own (Metal retains the IOSurface per-texture).
      entry.handle = (void *)surface;
      entry.width = (uint32_t)width;
      entry.height = (uint32_t)height;
      entry.fenceFd = -1; // CPU render already complete; no GPU wait fence.

      int token = sCaptureToken.fetch_add(1) + 1;
      rnwgpu::ElementCaptureRegistry::getInstance().store(token, entry);
      resolve(@(token));
    } @catch (NSException *e) {
      reject(@"E_CAPTURE", e.reason ?: @"capture failed", nil);
    }
  });
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
  return std::make_shared<facebook::react::NativeWebGPUModuleSpecJSI>(params);
}

@end
