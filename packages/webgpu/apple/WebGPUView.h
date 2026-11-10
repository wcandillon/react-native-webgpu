#pragma once

#import "RNWGUIKit.h"
#import <React/RCTViewComponentView.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * The Fabric component behind <Canvas>: a CAMetalLayer the canvas's frames
 * are presented into, by the app's render loop itself for a canvas presenting
 * direct (the default), or by the present thread, as the frame scheduler
 * decides, for a composited one (the presentation prop, see
 * SurfaceInfo::Presentation). The frames are rendered by the app, on whichever
 * runtime drives the canvas context.
 */
@interface WebGPUView : RCTViewComponentView
@end

NS_ASSUME_NONNULL_END
