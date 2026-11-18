#ifndef PACKAGES_WEBGPU_APPLE_RNWGUIKIT_H_
#define PACKAGES_WEBGPU_APPLE_RNWGUIKIT_H_

// Select UIKit or AppKit before importing either framework.
#import <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#if !TARGET_OS_OSX
typedef UIView RNWGPlatformView;
#else
typedef NSView RNWGPlatformView;
#endif

#endif // PACKAGES_WEBGPU_APPLE_RNWGUIKIT_H_
