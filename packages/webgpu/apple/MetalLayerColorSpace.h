#pragma once

#import <QuartzCore/CAMetalLayer.h>

#include "CanvasColorSpace.h"

namespace rnwgpu {

// Tags a new CAMetalLayer with the colorspace of the canvas, and sets its
// dynamic range and tone mapping from the canvas's toneMapping mode.
// Main thread only, where the platform view calls it as the canvas mounts (see
// -[MetalView configure]): the threads that configure and present the surface
// (JS, worklet runtimes) never touch a CATransaction.
void applyCAMetalLayerColorSpace(CAMetalLayer *layer,
                                 CanvasLayerColorSpace colorSpace);

} // namespace rnwgpu
