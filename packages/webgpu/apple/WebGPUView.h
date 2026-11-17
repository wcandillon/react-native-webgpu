#pragma once

#import "MetalView.h"
// RNWGUIKit selects AppKit for native macOS and UIKit for other Apple targets.
#import "RNWGUIKit.h"
#import <React/RCTViewComponentView.h>

NS_ASSUME_NONNULL_BEGIN

@interface WebGPUView : RCTViewComponentView
@end

NS_ASSUME_NONNULL_END
