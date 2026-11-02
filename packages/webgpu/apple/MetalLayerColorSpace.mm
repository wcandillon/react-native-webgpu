#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>

#include "CanvasColorSpace.h"
#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// WebGPU canvas values are encoded with the sRGB transfer function regardless
// of the texture format, and GPUCanvasConfiguration.colorSpace says which
// primaries they are in: "srgb" (the default) or "display-p3". The same shader
// output must display identically on bgra8unorm and rgba16float surfaces.
// Core Animation interprets a float-format layer with a nil colorspace as
// extended *linear* sRGB, which displays the same values noticeably brighter.
// Tagging the layer as (gamma-encoded) extended sRGB matches the browser
// behavior: identical colors, with the extra precision of float16. For
// "display-p3" the layer is tagged Display P3 instead, or its extended range
// variant on a float format, so the values reach a wide-gamut display without
// being converted from sRGB first.
//
// GPUCanvasConfiguration.toneMapping.mode says what happens to values above
// 1.0 on a float canvas. Core Animation keeps an extended colorspace inside
// the display's SDR range unless the layer asks for extended dynamic range
// content, so without it the values clip at white: that is "standard". For
// "extended" the layer opts in, and the values use the display's headroom
// (EDR) as they do in a browser. An 8-bit canvas cannot hold values above 1.0,
// so the flag only applies to a float format.
void applyCAMetalLayerColorSpace(void *nativeSurface,
                                 wgpu::TextureFormat format,
                                 CanvasColorSpace canvasColorSpace,
                                 CanvasToneMappingMode toneMappingMode) {
  CALayer *layer = (__bridge CALayer *)nativeSurface;
  if (![layer isKindOfClass:[CAMetalLayer class]]) {
    return;
  }
  auto metalLayer = static_cast<CAMetalLayer *>(layer);
  const bool isFloat = format == wgpu::TextureFormat::RGBA16Float;
  CFStringRef name = nullptr;
  if (canvasColorSpace == CanvasColorSpace::DisplayP3) {
    name = isFloat ? kCGColorSpaceExtendedDisplayP3 : kCGColorSpaceDisplayP3;
  } else if (isFloat) {
    name = kCGColorSpaceExtendedSRGB;
  }
  if (name != nullptr) {
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(name);
    metalLayer.colorspace = colorSpace;
    CGColorSpaceRelease(colorSpace);
  } else if (metalLayer.colorspace != nil) {
    // Restore the default (no color matching) when reconfiguring back to an
    // 8-bit sRGB canvas.
    metalLayer.colorspace = nil;
  }
  const bool extendedRange =
      isFloat && toneMappingMode == CanvasToneMappingMode::Extended;
  if (@available(iOS 16.0, tvOS 16.0, *)) {
    if (metalLayer.wantsExtendedDynamicRangeContent != extendedRange) {
      metalLayer.wantsExtendedDynamicRangeContent = extendedRange;
    }
  }
  // The change must be set synchronously so the first present already sees
  // it, and it must reach the render server. On a non-main thread (RN JS or
  // worklet runtime) the property lands in that thread's implicit
  // CATransaction, which may never commit on threads without a spinning
  // runloop, so flush it now. On the main thread the runloop commits it.
  if (!NSThread.isMainThread) {
    [CATransaction flush];
  }
}

} // namespace rnwgpu
