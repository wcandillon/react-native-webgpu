#pragma once

#include <Availability.h>
#include <TargetConditionals.h>

#import <QuartzCore/CAMetalLayer.h>

#include <string>

#include "CanvasColorSpace.h"

// preferredDynamicRange, contentsHeadroom and CADynamicRange* are declared
// only by the 26 SDKs (Xcode 26). @available guards the runtime, not the
// compilation, so older Xcodes need this too. The visionOS SDK keeps
// __IPHONE_OS_VERSION_MAX_ALLOWED at 17.0, hence its own macro.
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

// Tags a new CAMetalLayer with the colorspace of the canvas, and opts it into
// the display's extended dynamic range for an "extended" toneMapping mode.
// Main thread only, where the platform view calls it as the canvas mounts (see
// -[MetalView configure]): the threads that configure and present the surface
// (JS, worklet runtimes) never touch a CATransaction.
void applyCAMetalLayerColorSpace(CAMetalLayer *layer,
                                 CanvasLayerColorSpace colorSpace);

// Test only: what applyCAMetalLayerColorSpace set on the layer, as a JSON
// object (see PlatformContext::describeNativeSurfaceLayer). The getters of a
// model layer are safe off the main thread.
std::string describeCAMetalLayerColorSpace(CAMetalLayer *layer);

} // namespace rnwgpu
