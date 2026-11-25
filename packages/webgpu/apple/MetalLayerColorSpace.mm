#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

#include "MetalLayerColorSpace.h"

namespace rnwgpu {

#if RNWGPU_HAS_CA_DYNAMIC_RANGE
// The headroom an "extended" layer declares. A WebGPU canvas puts no ceiling
// on its values, so it asks for more than any display has (float16 holds far
// more). With tone mapping off the display clips at what it can show.
static constexpr CGFloat kExtendedContentsHeadroom = 16.0;
#endif

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
// canvas: "standard" clips them at SDR white, and "extended" shows them
// brighter, up to the display's headroom (EDR), as a browser does. An 8-bit
// canvas cannot hold values above 1.0, so the mode has no visible effect
// there.
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
  // The EDR properties do not exist on tvOS (the SDK marks them unavailable,
  // which @available cannot work around), so they are compiled only where
  // they exist. On tvOS "extended" behaves like "standard".
#if TARGET_OS_IOS || TARGET_OS_OSX || TARGET_OS_VISION
  if (@available(iOS 16.0, macOS 10.11, visionOS 1.0, *)) {
    layer.wantsExtendedDynamicRangeContent = extendedRange;
  }
#if RNWGPU_HAS_CA_TONE_MAP_MODE
  // The wantsExtendedDynamicRangeContent flag no longer clips by itself: a
  // float drawable is untagged, and Core Animation shows it as is, above 1.0
  // too, unless the layer asks for tone mapping. "standard" asks for it (to
  // SDR, see preferredDynamicRange below), and "extended" turns it off, so
  // values up to the display's headroom are shown as they are, not
  // compressed to fit the declared headroom.
  if (@available(iOS 18.0, macOS 15.0, visionOS 2.0, *)) {
    layer.toneMapMode =
        extendedRange ? CAToneMapModeNever : CAToneMapModeIfSupported;
  }
#endif
#if RNWGPU_HAS_CA_DYNAMIC_RANGE
  // Since iOS and macOS 26 the flag alone no longer lights EDR: the layer
  // asks for it with preferredDynamicRange, which needs a contentsHeadroom
  // above 1.0 to take effect. "standard" declares a headroom of 1.0: tone
  // mapped from an untagged headroom (0), its values below 1.0 come out
  // darker, while from 1.0 to SDR they are kept as is and clipped above.
  if (@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)) {
    layer.preferredDynamicRange =
        extendedRange ? CADynamicRangeHigh : CADynamicRangeStandard;
    layer.contentsHeadroom = extendedRange ? kExtendedContentsHeadroom : 1.0;
  }
#endif
#endif
  (void)extendedRange;
}

} // namespace rnwgpu
