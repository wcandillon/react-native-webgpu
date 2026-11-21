#pragma once

// RNWGUIKit selects AppKit for native macOS and UIKit for other Apple targets.
#import "RNWGUIKit.h"
#import <React/RCTViewComponentView.h>

NS_ASSUME_NONNULL_BEGIN

// NativeBufferView: displays an IOSurface that JS presented
// (RNWebGPU.presentNativeBuffer) as layer contents, with no swap chain and no
// GPU device on the display side. See NativeBufferViewRegistry.h.
@interface WebGPUNativeBufferView : RCTViewComponentView
@end

NS_ASSUME_NONNULL_END
