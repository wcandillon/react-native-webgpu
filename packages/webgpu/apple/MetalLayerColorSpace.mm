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
void applyCAMetalLayerColorSpace(void *nativeSurface,
                                 wgpu::TextureFormat format,
                                 CanvasColorSpace canvasColorSpace) {
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
