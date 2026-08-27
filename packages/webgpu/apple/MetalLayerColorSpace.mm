#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// WebGPU canvas values are sRGB-encoded regardless of the texture format
// (GPUCanvasConfiguration.colorSpace defaults to "srgb"), so the same shader
// output must display identically on bgra8unorm and rgba16float surfaces.
// Core Animation interprets a float-format layer with a nil colorspace as
// extended *linear* sRGB, which displays the same values noticeably brighter.
// Tagging the layer as (gamma-encoded) extended sRGB matches the browser
// behavior: identical colors, with the extra precision of float16.
void applyCAMetalLayerColorSpace(void *nativeSurface,
                                 wgpu::TextureFormat format) {
#if DEBUG
  static dispatch_once_t threadCheck;
  dispatch_once(&threadCheck, ^{
    NSLog(@"[cametal-repro] CAMetalLayer main-thread check: %@",
          NSThread.isMainThread ? @"PASS" : @"FAIL");
  });
  NSCAssert(NSThread.isMainThread,
            @"CAMetalLayer mutations must run on the main thread");
#endif
  CALayer *layer = (__bridge CALayer *)nativeSurface;
  if (![layer isKindOfClass:[CAMetalLayer class]]) {
    return;
  }
  auto metalLayer = static_cast<CAMetalLayer *>(layer);
  if (format == wgpu::TextureFormat::RGBA16Float) {
    CGColorSpaceRef colorSpace =
        CGColorSpaceCreateWithName(kCGColorSpaceExtendedSRGB);
    metalLayer.colorspace = colorSpace;
    CGColorSpaceRelease(colorSpace);
  } else if (metalLayer.colorspace != nil) {
    // Restore the default (no color matching) when reconfiguring back to an
    // 8-bit format.
    metalLayer.colorspace = nil;
  }
}

} // namespace rnwgpu
