#include "ApplePlatformContext.h"

#include <TargetConditionals.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#import <React/RCTBlobManager.h>
#import <React/RCTBridge+Private.h>
#import <ReactCommon/RCTTurboModule.h>

#include "AppleVideoPlayer.h"
#import "RNWGUIKit.h"

#include "RNWebGPUManager.h"
#include "WebGPUModule.h"

#include <cmath>
#include <string>
#include <utility>

namespace rnwgpu {

void checkIfUsingSimulatorWithAPIValidation() {
#if TARGET_OS_SIMULATOR
  NSDictionary *environment = [[NSProcessInfo processInfo] environment];
  NSString *metalDeviceWrapperType = environment[@"METAL_DEVICE_WRAPPER_TYPE"];

  if ([metalDeviceWrapperType isEqualToString:@"1"]) {
    throw std::runtime_error(
        "To use React Native WebGPU project on the iOS simulator, you need to "
        "disable the Metal validation API. In 'Edit Scheme,' uncheck 'Metal "
        "Validation.'");
  }
#endif
}

ApplePlatformContext::ApplePlatformContext(ViewLookup viewLookup)
    : _viewLookup(std::move(viewLookup)) {
  checkIfUsingSimulatorWithAPIValidation();
}

namespace {

// Rasterize `request` on the main thread into tightly packed, premultiplied
// BGRA8 pixels. Throws std::runtime_error with a user-facing message.
ImageData snapshotViewOnMainThread(const ApplePlatformContext::ViewLookup &lookup,
                                   const ViewSnapshotRequest &request) {
  if (!lookup) {
    throw std::runtime_error(
        "drawElementImageToTexture: no view registry is available");
  }
  RNWGPlatformView *view = (__bridge RNWGPlatformView *)lookup(request.viewTag);
  if (view == nil) {
    throw std::runtime_error(
        "drawElementImageToTexture: no native view found for tag " +
        std::to_string(request.viewTag) +
        " (is the view mounted, and rendered with collapsable={false}?)");
  }
  const CGRect bounds = view.bounds;
  if (bounds.size.width <= 0 || bounds.size.height <= 0) {
    throw std::runtime_error(
        "drawElementImageToTexture: the view has no size yet");
  }

  // Source rectangle in points (view space); the whole view by default.
  const double sourceX = request.sourceX;
  const double sourceY = request.sourceY;
  const double sourceWidth =
      request.sourceWidth > 0 ? request.sourceWidth : bounds.size.width;
  const double sourceHeight =
      request.sourceHeight > 0 ? request.sourceHeight : bounds.size.height;

  // Natural pixel size: points times the screen scale the view is shown at.
#if !TARGET_OS_OSX
  CGFloat scale = view.window.screen.scale;
  if (scale <= 0) {
    scale = UIScreen.mainScreen.scale;
  }
#else
  CGFloat scale = view.window.backingScaleFactor;
  if (scale <= 0) {
    scale = NSScreen.mainScreen.backingScaleFactor;
  }
#endif
  const uint32_t width =
      request.width > 0
          ? request.width
          : static_cast<uint32_t>(std::llround(sourceWidth * scale));
  const uint32_t height =
      request.height > 0
          ? request.height
          : static_cast<uint32_t>(std::llround(sourceHeight * scale));
  if (width == 0 || height == 0) {
    throw std::runtime_error(
        "drawElementImageToTexture: the source rectangle is empty");
  }

  // The view's bounds are drawn scaled so that the source rectangle covers
  // the whole output.
  const CGFloat scaleX = static_cast<CGFloat>(width) / sourceWidth;
  const CGFloat scaleY = static_cast<CGFloat>(height) / sourceHeight;
  const CGRect drawRect =
      CGRectMake(-sourceX * scaleX, -sourceY * scaleY,
                 bounds.size.width * scaleX, bounds.size.height * scaleY);

  CGImageRef cgImage = NULL;
#if !TARGET_OS_OSX
  // drawViewHierarchyInRect renders what is on screen (including Metal /
  // CAMetalLayer content), which is what "the element as displayed" means.
  // afterScreenUpdates:YES flushes pending layout and layer changes first, so
  // a snapshot taken right after a state change reflects that change.
  UIGraphicsImageRendererFormat *format =
      [UIGraphicsImageRendererFormat defaultFormat];
  format.scale = 1.0;
  format.opaque = NO;
  format.preferredRange = UIGraphicsImageRendererFormatRangeStandard;
  UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc]
      initWithSize:CGSizeMake(width, height)
            format:format];
  UIImage *image = [renderer
      imageWithActions:^(UIGraphicsImageRendererContext *_Nonnull context) {
        [view drawViewHierarchyInRect:drawRect afterScreenUpdates:YES];
      }];
  cgImage = image.CGImage;
  if (cgImage != NULL) {
    CGImageRetain(cgImage);
  }
#else
  NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:bounds];
  if (rep != nil) {
    [view cacheDisplayInRect:bounds toBitmapImageRep:rep];
    cgImage = rep.CGImage;
    if (cgImage != NULL) {
      CGImageRetain(cgImage);
    }
  }
#endif
  if (cgImage == NULL) {
    throw std::runtime_error(
        "drawElementImageToTexture: the view could not be rendered");
  }

  // Normalize into premultiplied BGRA8 with no row padding, whatever the
  // renderer produced.
  ImageData result;
  result.width = width;
  result.height = height;
  result.format = wgpu::TextureFormat::BGRA8Unorm;
  result.premultiplied = true;
  result.data.assign(static_cast<size_t>(width) * height * 4, 0);
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      result.data.data(), width, height, 8, static_cast<size_t>(width) * 4,
      colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
  CGColorSpaceRelease(colorSpace);
  if (context == NULL) {
    CGImageRelease(cgImage);
    throw std::runtime_error(
        "drawElementImageToTexture: could not allocate the pixel buffer");
  }
#if !TARGET_OS_OSX
  // The renderer already produced exactly width x height pixels.
  const CGRect imageRect = CGRectMake(0, 0, width, height);
#else
  // The cached display covers the whole view at the backing scale; draw it so
  // that the source rectangle fills the output. CoreGraphics' origin is the
  // bottom-left corner, hence the flipped vertical offset.
  const CGRect imageRect = CGRectMake(
      -sourceX * scaleX,
      -(bounds.size.height - sourceY - sourceHeight) * scaleY,
      bounds.size.width * scaleX, bounds.size.height * scaleY);
#endif
  CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextDrawImage(context, imageRect, cgImage);
  CGContextRelease(context);
  CGImageRelease(cgImage);
  return result;
}

} // namespace

void ApplePlatformContext::snapshotView(
    const ViewSnapshotRequest &request,
    std::function<void(ImageData)> onSuccess,
    std::function<void(std::string)> onError) {
  // Copies: the block outlives this call.
  ViewLookup lookup = _viewLookup;
  ViewSnapshotRequest snapshotRequest = request;
  dispatch_async(dispatch_get_main_queue(), ^{
    @autoreleasepool {
      try {
        onSuccess(snapshotViewOnMainThread(lookup, snapshotRequest));
      } catch (const std::exception &error) {
        onError(error.what());
      } catch (...) {
        onError("drawElementImageToTexture: unknown native error");
      }
    }
  });
}

wgpu::Surface ApplePlatformContext::makeSurface(wgpu::Instance instance,
                                                void *surface, int width,
                                                int height) {
  wgpu::SurfaceSourceMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = surface;
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  return instance.CreateSurface(&surfaceDescriptor);
}

static std::span<const uint8_t> nsDataToSpan(NSData *data) {
  return {static_cast<const uint8_t *>(data.bytes), data.length};
}

ImageData ApplePlatformContext::createImageBitmap(std::string blobId,
                                                  double offset, double size) {
  RCTBlobManager *blobManager =
      [[RCTBridge currentBridge] moduleForClass:RCTBlobManager.class];
  NSData *blobData =
      [blobManager resolve:[NSString stringWithUTF8String:blobId.c_str()]
                    offset:(long)offset
                      size:(long)size];

  if (!blobData) {
    throw std::runtime_error("Couldn't retrieve blob data");
  }

  return createImageBitmapFromData(nsDataToSpan(blobData));
}

void ApplePlatformContext::createImageBitmapAsync(
    std::string blobId, double offset, double size,
    std::function<void(ImageData)> onSuccess,
    std::function<void(std::string)> onError) {
  // Resolve blob on current thread (requires RCTBridge access)
  RCTBlobManager *blobManager =
      [[RCTBridge currentBridge] moduleForClass:RCTBlobManager.class];
  NSData *blobData =
      [blobManager resolve:[NSString stringWithUTF8String:blobId.c_str()]
                    offset:(long)offset
                      size:(long)size];

  if (!blobData) {
    onError("Couldn't retrieve blob data");
    return;
  }

  // blobData is alive during this synchronous call;
  // createImageBitmapFromDataAsync copies the span before dispatching
  createImageBitmapFromDataAsync(nsDataToSpan(blobData), std::move(onSuccess),
                                 std::move(onError));
}

ImageData
ApplePlatformContext::createImageBitmapFromData(std::span<const uint8_t> data) {
  // All formats are decoded through ImageIO. Apple's imaging stack always
  // premultiplies alpha at decode, so the result is flagged premultiplied and
  // createImageBitmap / copyExternalImageToTexture convert from there. This
  // means premultiplyAlpha "none" is a lossy round trip for low-alpha pixels
  // (as it is on Android); the snapshot suites compare with pixelmatch
  // tolerance to absorb it.
  NSData *nsData =
      [NSData dataWithBytesNoCopy:const_cast<uint8_t *>(data.data())
                           length:data.size()
                     freeWhenDone:NO];

  CGImageSourceRef imageSource =
      CGImageSourceCreateWithData((__bridge CFDataRef)nsData, NULL);
  if (imageSource == NULL) {
    throw std::runtime_error("Couldn't create image source");
  }
  NSDictionary *decodeOptions = @{(id)kCGImageSourceShouldCache : @NO};
  CGImageRef cgImage = CGImageSourceCreateImageAtIndex(
      imageSource, 0, (__bridge CFDictionaryRef)decodeOptions);
  CFRelease(imageSource);
  if (cgImage == NULL) {
    throw std::runtime_error("Couldn't decode image");
  }

  size_t width = CGImageGetWidth(cgImage);
  size_t height = CGImageGetHeight(cgImage);
  size_t bytesPerRow = width * 4;

  ImageData result;
  result.width = static_cast<int>(width);
  result.height = static_cast<int>(height);
  result.data.resize(height * bytesPerRow);
  result.format = wgpu::TextureFormat::RGBA8Unorm;

  // The draw premultiplies alpha; flag the result premultiplied so
  // createImageBitmap and copyExternalImageToTexture convert consistently.
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      result.data.data(), width, height, 8, bytesPerRow, colorSpace,
      kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);
  CGImageRelease(cgImage);

  result.premultiplied = true;
  return result;
}

void ApplePlatformContext::createImageBitmapFromDataAsync(
    std::span<const uint8_t> data, std::function<void(ImageData)> onSuccess,
    std::function<void(std::string)> onError) {
  // Copy span data into shared_ptr so the dispatch_async block owns the memory
  auto ownedData =
      std::make_shared<std::vector<uint8_t>>(data.begin(), data.end());

  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    @autoreleasepool {
      try {
        auto result = createImageBitmapFromData(*ownedData);
        onSuccess(std::move(result));
      } catch (const std::exception &e) {
        onError(e.what());
      }
    }
  });
}

VideoFrameHandle
ApplePlatformContext::loadVideoFrame(const std::string &path) {
  NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
  NSURL *url = [nsPath hasPrefix:@"file://"]
                   ? [NSURL URLWithString:nsPath]
                   : [NSURL fileURLWithPath:nsPath];
  AVURLAsset *asset = [AVURLAsset assetWithURL:url];

  NSArray<AVAssetTrack *> *videoTracks =
      [asset tracksWithMediaType:AVMediaTypeVideo];
  if (videoTracks.count == 0) {
    throw std::runtime_error("loadVideoFrame: no video track in file");
  }
  AVAssetTrack *videoTrack = videoTracks.firstObject;

  NSError *error = nil;
  AVAssetReader *reader = [AVAssetReader assetReaderWithAsset:asset
                                                        error:&error];
  if (error || !reader) {
    throw std::runtime_error(
        std::string("loadVideoFrame: AVAssetReader init failed: ") +
        [[error localizedDescription] UTF8String]);
  }

  NSDictionary *outputSettings = @{
    (NSString *)kCVPixelBufferPixelFormatTypeKey :
        @(kCVPixelFormatType_32BGRA),
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  AVAssetReaderTrackOutput *output =
      [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:videoTrack
                                                 outputSettings:outputSettings];
  output.alwaysCopiesSampleData = NO;
  if (![reader canAddOutput:output]) {
    throw std::runtime_error("loadVideoFrame: cannot add output");
  }
  [reader addOutput:output];

  if (![reader startReading]) {
    throw std::runtime_error(
        std::string("loadVideoFrame: startReading failed: ") +
        [[reader.error localizedDescription] UTF8String]);
  }

  CMSampleBufferRef sampleBuffer = [output copyNextSampleBuffer];
  if (!sampleBuffer) {
    throw std::runtime_error("loadVideoFrame: no sample buffer");
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer) {
    CFRelease(sampleBuffer);
    throw std::runtime_error("loadVideoFrame: no pixel buffer");
  }

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface) {
    CFRelease(sampleBuffer);
    throw std::runtime_error(
        "loadVideoFrame: pixel buffer is not IOSurface-backed");
  }

  // Retain the IOSurface so it survives past the sample buffer's lifetime.
  CFRetain(ioSurface);

  VideoFrameHandle handle;
  handle.handle = (void *)ioSurface;
  handle.width = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
  handle.height = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
  handle.deleter = [ioSurface]() { CFRelease(ioSurface); };

  CFRelease(sampleBuffer);
  [reader cancelReading];

  return handle;
}

std::unique_ptr<IVideoPlayer>
ApplePlatformContext::createVideoPlayer(const std::string &path,
                                         VideoPixelFormat format) {
  return createAppleVideoPlayer(path, format);
}

std::string ApplePlatformContext::writeTestVideoFile() {
  return writeAppleTestVideoFile();
}

VideoFrameHandle ApplePlatformContext::wrapNativeBuffer(void *pointer) {
  return wrapCVPixelBuffer(static_cast<CVPixelBufferRef>(pointer));
}

VideoFrameHandle
ApplePlatformContext::createTestVideoFrame(uint32_t width, uint32_t height) {
  NSDictionary *attrs = @{
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVPixelBufferRef pixelBuffer = NULL;
  CVReturn err = CVPixelBufferCreate(
      kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA,
      (__bridge CFDictionaryRef)attrs, &pixelBuffer);
  if (err != kCVReturnSuccess || !pixelBuffer) {
    throw std::runtime_error("createTestVideoFrame: CVPixelBufferCreate "
                             "failed");
  }

  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  uint8_t *base =
      static_cast<uint8_t *>(CVPixelBufferGetBaseAddress(pixelBuffer));
  size_t rowBytes = CVPixelBufferGetBytesPerRow(pixelBuffer);
  for (uint32_t y = 0; y < height; ++y) {
    uint8_t *row = base + y * rowBytes;
    for (uint32_t x = 0; x < width; ++x) {
      // RGB gradient + diagonal stripes, in BGRA byte order.
      uint8_t r = static_cast<uint8_t>((x * 255) / std::max(width - 1, 1u));
      uint8_t g = static_cast<uint8_t>((y * 255) / std::max(height - 1, 1u));
      uint8_t b = static_cast<uint8_t>(((x + y) & 0x20) ? 220 : 30);
      row[x * 4 + 0] = b;
      row[x * 4 + 1] = g;
      row[x * 4 + 2] = r;
      row[x * 4 + 3] = 0xFF;
    }
  }
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface) {
    CFRelease(pixelBuffer);
    throw std::runtime_error(
        "createTestVideoFrame: pixel buffer is not IOSurface-backed");
  }

  VideoFrameHandle handle;
  handle.handle = (void *)ioSurface;
  handle.width = width;
  handle.height = height;
  handle.deleter = [pixelBuffer]() { CFRelease(pixelBuffer); };
  return handle;
}

} // namespace rnwgpu
