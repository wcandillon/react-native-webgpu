#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

#include <Availability.h>
#include <TargetConditionals.h>

#include "MetalLayerColorSpace.h"

// @available guards the runtime, not the compilation: these say which SDK
// declares the APIs, so older Xcodes keep building. The visionOS SDK keeps
// __IPHONE_OS_VERSION_MAX_ALLOWED at 17.0, hence its own macro.
// CALayer.toneMapMode: iOS 18 / macOS 15 / visionOS 2 SDKs (Xcode 16).
#if (TARGET_OS_IOS && defined(__IPHONE_OS_VERSION_MAX_ALLOWED) &&              \
     __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000) ||                             \
    (TARGET_OS_VISION && defined(__VISION_OS_VERSION_MAX_ALLOWED) &&           \
     __VISION_OS_VERSION_MAX_ALLOWED >= 20000) ||                              \
    (TARGET_OS_OSX && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&               \
     __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000)
#define RNWGPU_HAS_CA_TONE_MAP_MODE 1
#else
#define RNWGPU_HAS_CA_TONE_MAP_MODE 0
#endif
// preferredDynamicRange, contentsHeadroom, CADynamicRange*: 26 SDKs (Xcode 26).
#if (TARGET_OS_IOS && defined(__IPHONE_OS_VERSION_MAX_ALLOWED) &&              \
     __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000) ||                             \
    (TARGET_OS_VISION && defined(__VISION_OS_VERSION_MAX_ALLOWED) &&           \
     __VISION_OS_VERSION_MAX_ALLOWED >= 260000) ||                             \
    (TARGET_OS_OSX && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&               \
     __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000)
#define RNWGPU_HAS_CA_DYNAMIC_RANGE 1
#else
#define RNWGPU_HAS_CA_DYNAMIC_RANGE 0
#endif

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
  [[maybe_unused]] const bool extendedRange =
      colorSpace.toneMappingMode == CanvasToneMappingMode::Extended;
  // The EDR properties do not exist on tvOS (the SDK marks them unavailable,
  // which @available cannot work around), so they are compiled only where
  // they exist. On tvOS "extended" behaves like "standard".
#if TARGET_OS_IOS || TARGET_OS_OSX || TARGET_OS_VISION
  if (@available(iOS 16.0, macOS 10.11, visionOS 1.0, *)) {
    layer.wantsExtendedDynamicRangeContent = extendedRange;
  }
#endif
  // The flag alone does not keep "standard" in SDR: while any layer on screen
  // shows EDR, Core Animation clamps all content to the display's headroom
  // rather than 1.0 (see wantsExtendedDynamicRangeContent in CAMetalLayer.h).
  // "standard" is tone mapped to SDR; "extended" is not tone mapped, so it is
  // shown up to the display's headroom, never compressed.
#if RNWGPU_HAS_CA_TONE_MAP_MODE
  if (@available(iOS 18.0, macOS 15.0, visionOS 2.0, *)) {
    layer.toneMapMode =
        extendedRange ? CAToneMapModeNever : CAToneMapModeIfSupported;
  }
#endif
  // The 26 SDKs replace the flag with preferredDynamicRange, which lights EDR
  // only for a contentsHeadroom above 1.0. "extended" declares more than any
  // display has; "standard" declares 1.0 (SDR), so its tone mapping keeps
  // values up to 1.0 as they are and clips above.
#if RNWGPU_HAS_CA_DYNAMIC_RANGE
  if (@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)) {
    layer.preferredDynamicRange =
        extendedRange ? CADynamicRangeHigh : CADynamicRangeStandard;
    layer.contentsHeadroom = extendedRange ? 16.0 : 1.0;
  }
#endif
}

} // namespace rnwgpu
