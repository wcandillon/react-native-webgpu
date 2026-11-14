#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

#include "MetalLayerColorSpace.h"

namespace rnwgpu {

// WebGPU canvas values are encoded with the sRGB transfer function regardless
// of the texture format, and the canvas's colorSpace says which primaries they
// are in: "srgb" (the default) or "display-p3". The same shader output must
// display identically on bgra8unorm and rgba16float surfaces. Core Animation
// interprets a float-format layer with a nil colorspace as extended *linear*
// sRGB, which displays the same values noticeably brighter. The layer is
// tagged before the format is known (configure() comes later, from JS), so it
// gets the gamma-encoded extended variant of its gamut: on an 8-bit format it
// reads the same values as the plain colorspace, and on a float format it
// gives identical colors with the extra precision of float16, as in a browser.
// For "display-p3" the values reach a wide-gamut display without being
// converted from sRGB first.
//
// The toneMapping mode says what happens to values above 1.0 on a float
// canvas. Core Animation keeps an extended colorspace inside the display's
// SDR range unless the layer asks for extended dynamic range content, so
// without it the values clip at white: that is "standard". For "extended" the
// layer opts in, and the values use the display's headroom (EDR) as they do in
// a browser. An 8-bit canvas cannot hold values above 1.0, so the flag has no
// visible effect there.
void applyCAMetalLayerColorSpace(CAMetalLayer *layer,
                                 CanvasLayerColorSpace colorSpace) {
  // Main thread only: the properties land in the calling thread's implicit
  // CATransaction, which the main run loop commits. Committing it from any
  // other thread ([CATransaction flush]) commits the whole layer tree there,
  // and lays out whatever UIKit view has a layout pending, off the main
  // thread (react-native-skia #4474).
  NSCAssert(NSThread.isMainThread,
            @"applyCAMetalLayerColorSpace must be called on the main thread");
  CGColorSpaceRef cgColorSpace = CGColorSpaceCreateWithName(
      colorSpace.colorSpace == CanvasColorSpace::DisplayP3
          ? kCGColorSpaceExtendedDisplayP3
          : kCGColorSpaceExtendedSRGB);
  layer.colorspace = cgColorSpace;
  CGColorSpaceRelease(cgColorSpace);
  const bool extendedRange =
      colorSpace.toneMappingMode == CanvasToneMappingMode::Extended;
  // The property does not exist on tvOS (the SDK marks it unavailable, which
  // @available cannot work around), so it is compiled only where it exists.
  // On tvOS "extended" behaves like "standard".
#if TARGET_OS_IOS || TARGET_OS_OSX || TARGET_OS_VISION
  if (@available(iOS 16.0, macOS 10.11, visionOS 1.0, *)) {
    layer.wantsExtendedDynamicRangeContent = extendedRange;
  }
#else
  (void)extendedRange;
#endif
}

} // namespace rnwgpu
