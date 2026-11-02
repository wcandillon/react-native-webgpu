#include "ApplePlatformContext.h"

#include <TargetConditionals.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#import <React/RCTBlobManager.h>
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

ApplePlatformContext::ApplePlatformContext(ViewLookup viewLookup,
                                           BlobManagerLookup blobManagerLookup)
    : _viewLookup(std::move(viewLookup)),
      _blobManagerLookup(std::move(blobManagerLookup)) {
  checkIfUsingSimulatorWithAPIValidation();
}

namespace {

// A view that UIKit declined to render (it is not in a window). The pixel
// path would fail the same way, so there is no point falling back to it.
struct ViewNotRenderable : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// The view, the output size and the rectangle to draw it in, resolved from a
// request on the main thread. Throws std::runtime_error with a user-facing
// message.
struct ResolvedSnapshot {
  RNWGPlatformView *view = nil;
  CGRect bounds = CGRectZero;
  // The source rectangle in points (view space).
  double sourceX = 0;
  double sourceY = 0;
  double sourceWidth = 0;
  double sourceHeight = 0;
  // The output size in pixels.
  uint32_t width = 0;
  uint32_t height = 0;
  CGFloat scaleX = 1;
  CGFloat scaleY = 1;
  // The view's bounds, scaled and offset so that the source rectangle covers
  // the whole output, in pixels and with UIKit's top-left origin.
  CGRect drawRect = CGRectZero;
};

ResolvedSnapshot resolveSnapshot(const ApplePlatformContext::ViewLookup &lookup,
                                 const ViewSnapshotRequest &request) {
  if (!lookup) {
    throw std::runtime_error(
        "drawElementImageToTexture: no view registry is available");
  }
  ResolvedSnapshot snapshot;
  snapshot.view = (__bridge RNWGPlatformView *)lookup(request.viewTag);
  if (snapshot.view == nil) {
    throw std::runtime_error(
        "drawElementImageToTexture: no native view found for tag " +
        std::to_string(request.viewTag) +
        " (is the view mounted, and rendered with collapsable={false}?)");
  }
  snapshot.bounds = snapshot.view.bounds;
  const CGRect bounds = snapshot.bounds;
  if (bounds.size.width <= 0 || bounds.size.height <= 0) {
    throw std::runtime_error(
        "drawElementImageToTexture: the view has no size yet");
  }

  // Source rectangle in points (view space); the whole view by default.
  snapshot.sourceX = request.sourceX;
  snapshot.sourceY = request.sourceY;
  snapshot.sourceWidth =
      request.sourceWidth > 0 ? request.sourceWidth : bounds.size.width;
  snapshot.sourceHeight =
      request.sourceHeight > 0 ? request.sourceHeight : bounds.size.height;

  // Natural pixel size: points times the screen scale the view is shown at.
#if !TARGET_OS_OSX
  CGFloat scale = snapshot.view.window.screen.scale;
  if (scale <= 0) {
    scale = UIScreen.mainScreen.scale;
  }
#else
  CGFloat scale = snapshot.view.window.backingScaleFactor;
  if (scale <= 0) {
    scale = NSScreen.mainScreen.backingScaleFactor;
  }
#endif
  snapshot.width =
      request.width > 0
          ? request.width
          : static_cast<uint32_t>(std::llround(snapshot.sourceWidth * scale));
  snapshot.height =
      request.height > 0
          ? request.height
          : static_cast<uint32_t>(std::llround(snapshot.sourceHeight * scale));
  if (snapshot.width == 0 || snapshot.height == 0) {
    throw std::runtime_error(
        "drawElementImageToTexture: the source rectangle is empty");
  }

  // The view's bounds are drawn scaled so that the source rectangle covers
  // the whole output.
  snapshot.scaleX = static_cast<CGFloat>(snapshot.width) / snapshot.sourceWidth;
  snapshot.scaleY =
      static_cast<CGFloat>(snapshot.height) / snapshot.sourceHeight;
  snapshot.drawRect = CGRectMake(-snapshot.sourceX * snapshot.scaleX,
                                 -snapshot.sourceY * snapshot.scaleY,
                                 bounds.size.width * snapshot.scaleX,
                                 bounds.size.height * snapshot.scaleY);
  return snapshot;
}

// A premultiplied BGRA8 Core Graphics context over `pixels`, the layout both
// snapshot paths produce. Returns NULL when Core Graphics rejects it.
CGContextRef createBgraContext(void *pixels, uint32_t width, uint32_t height,
                               size_t bytesPerRow) {
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      pixels, width, height, 8, bytesPerRow, colorSpace,
      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
  CGColorSpaceRelease(colorSpace);
  return context;
}

#if !TARGET_OS_OSX
// Draws the view hierarchy through `context`, a bitmap context of exactly
// `snapshot.width` x `snapshot.height` pixels (Core Graphics orientation; the
// flip to UIKit's top-left origin is applied here).
//
// drawViewHierarchyInRect renders what is on screen (including Metal /
// CAMetalLayer content), which is what "the element as displayed" means.
// afterScreenUpdates:YES flushes pending layout and layer changes first, so a
// snapshot taken right after a state change reflects that change; NO draws
// the last committed frame and skips that round trip to the render server.
//
// drawViewHierarchyInRect returns NO and draws nothing when the view is not
// in a window (e.g. a screen pushed under another one). The output would then
// be fully transparent, so that is reported as a failure instead.
void drawViewHierarchy(const ResolvedSnapshot &snapshot, CGContextRef context,
                       bool afterScreenUpdates) {
  CGContextSaveGState(context);
  CGContextTranslateCTM(context, 0, snapshot.height);
  CGContextScaleCTM(context, 1, -1);
  UIGraphicsPushContext(context);
  const BOOL drawn =
      [snapshot.view drawViewHierarchyInRect:snapshot.drawRect
                          afterScreenUpdates:afterScreenUpdates ? YES : NO];
  UIGraphicsPopContext();
  CGContextRestoreGState(context);
  if (!drawn) {
    throw ViewNotRenderable(
        "drawElementImageToTexture: the view could not be rendered (is it "
        "attached to a window?)");
  }
}

// Releases a pixel buffer when it goes out of scope.
struct PixelBufferRelease {
  CVPixelBufferRef buffer;
  ~PixelBufferRelease() {
    if (buffer != NULL) {
      CFRelease(buffer);
    }
  }
};

// The zero-copy path: renders the view into an IOSurface-backed BGRA pixel
// buffer, which the caller imports as shared texture memory and draws into
// the destination on the GPU (VideoFrameBlit, which also converts the
// premultiplied pixels to straight alpha when asked). UIKit rasterizes
// straight into the surface's memory through a Core Graphics context, so the
// pixels are never copied or converted on the CPU. Throws on an allocation
// failure, which the caller answers with the pixel path.
VideoFrameHandle renderViewIntoSurface(const ResolvedSnapshot &snapshot,
                                       bool afterScreenUpdates) {
  NSDictionary *attributes = @{
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVPixelBufferRef pixelBuffer = NULL;
  const CVReturn created =
      CVPixelBufferCreate(kCFAllocatorDefault, snapshot.width, snapshot.height,
                          kCVPixelFormatType_32BGRA,
                          (__bridge CFDictionaryRef)attributes, &pixelBuffer);
  if (created != kCVReturnSuccess || pixelBuffer == NULL) {
    throw std::runtime_error(
        "drawElementImageToTexture: could not allocate an IOSurface for the "
        "view (CVPixelBufferCreate returned " +
        std::to_string(created) + ")");
  }
  // wrapCVPixelBuffer retains its own reference for the frame.
  PixelBufferRelease release{pixelBuffer};

  if (CVPixelBufferLockBaseAddress(pixelBuffer, 0) != kCVReturnSuccess) {
    throw std::runtime_error(
        "drawElementImageToTexture: could not lock the IOSurface");
  }
  CGContextRef context = createBgraContext(
      CVPixelBufferGetBaseAddress(pixelBuffer), snapshot.width, snapshot.height,
      CVPixelBufferGetBytesPerRow(pixelBuffer));
  if (context == NULL) {
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    throw std::runtime_error(
        "drawElementImageToTexture: could not draw into the IOSurface");
  }
  try {
    // A fresh surface holds whatever was in that memory, and the view may be
    // transparent.
    CGContextClearRect(context,
                       CGRectMake(0, 0, snapshot.width, snapshot.height));
    drawViewHierarchy(snapshot, context, afterScreenUpdates);
  } catch (...) {
    CGContextRelease(context);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    throw;
  }
  CGContextRelease(context);
  // Unlocking publishes the CPU writes to the GPU.
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  return wrapCVPixelBuffer(pixelBuffer);
}
#endif

// The pixel path: rasterizes the view into tightly packed, premultiplied
// BGRA8 pixels that the caller uploads with writeTexture.
ImageData renderViewIntoPixels(const ResolvedSnapshot &snapshot,
                               bool afterScreenUpdates) {
  ImageData result;
  result.width = snapshot.width;
  result.height = snapshot.height;
  result.format = wgpu::TextureFormat::BGRA8Unorm;
  result.premultiplied = true;
  // Zeroed: the view may be transparent.
  result.data.assign(static_cast<size_t>(snapshot.width) * snapshot.height * 4,
                     0);
  CGContextRef context =
      createBgraContext(result.data.data(), snapshot.width, snapshot.height,
                        static_cast<size_t>(snapshot.width) * 4);
  if (context == NULL) {
    throw std::runtime_error(
        "drawElementImageToTexture: could not allocate the pixel buffer");
  }
#if !TARGET_OS_OSX
  // UIKit draws straight into the vector; no intermediate image.
  try {
    drawViewHierarchy(snapshot, context, afterScreenUpdates);
  } catch (...) {
    CGContextRelease(context);
    throw;
  }
#else
  (void)afterScreenUpdates;
  CGImageRef cgImage = NULL;
  NSBitmapImageRep *rep =
      [snapshot.view bitmapImageRepForCachingDisplayInRect:snapshot.bounds];
  if (rep != nil) {
    [snapshot.view cacheDisplayInRect:snapshot.bounds toBitmapImageRep:rep];
    cgImage = rep.CGImage;
    if (cgImage != NULL) {
      CGImageRetain(cgImage);
    }
  }
  if (cgImage == NULL) {
    CGContextRelease(context);
    throw std::runtime_error(
        "drawElementImageToTexture: the view could not be rendered");
  }
  // The cached display covers the whole view at the backing scale; draw it so
  // that the source rectangle fills the output. CoreGraphics' origin is the
  // bottom-left corner, hence the flipped vertical offset.
  const CGRect imageRect =
      CGRectMake(-snapshot.sourceX * snapshot.scaleX,
                 -(snapshot.bounds.size.height - snapshot.sourceY -
                   snapshot.sourceHeight) *
                     snapshot.scaleY,
                 snapshot.bounds.size.width * snapshot.scaleX,
                 snapshot.bounds.size.height * snapshot.scaleY);
  CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextDrawImage(context, imageRect, cgImage);
  CGImageRelease(cgImage);
#endif
  CGContextRelease(context);
  return result;
}

} // namespace

// On iOS a caller that can draw a GPU frame (allowFrame) gets the view
// rendered into an IOSurface; everything else, and macOS, gets pixels.
void ApplePlatformContext::snapshotView(
    const ViewSnapshotRequest &request, std::function<void(ImageData)> onPixels,
    std::function<void(VideoFrameHandle)> onFrame,
    std::function<void(std::string)> onError) {
  // Copies: the block outlives this call.
  ViewLookup lookup = _viewLookup;
  ViewSnapshotRequest snapshotRequest = request;
  dispatch_async(dispatch_get_main_queue(), ^{
    @autoreleasepool {
      try {
        const ResolvedSnapshot snapshot =
            resolveSnapshot(lookup, snapshotRequest);
#if !TARGET_OS_OSX
        if (snapshotRequest.allowFrame) {
          try {
            onFrame(renderViewIntoSurface(snapshot,
                                          snapshotRequest.afterScreenUpdates));
            return;
          } catch (const ViewNotRenderable &) {
            throw;
          } catch (const std::exception &error) {
            // An IOSurface could not be set up; the pixel path draws the
            // same view.
            NSLog(@"[WebGPU] drawElementImageToTexture: %s; reading the "
                  @"pixels back instead",
                  error.what());
          }
        }
#else
        (void)onFrame;
#endif
        onPixels(
            renderViewIntoPixels(snapshot, snapshotRequest.afterScreenUpdates));
      } catch (const std::exception &error) {
        onError(error.what());
      } catch (...) {
        onError("drawElementImageToTexture: unknown native error");
      }
    }
  });
}

static std::span<const uint8_t> nsDataToSpan(NSData *data) {
  return {static_cast<const uint8_t *>(data.bytes), data.length};
}

ImageData ApplePlatformContext::createImageBitmap(std::string blobId,
                                                  double offset, double size) {
  RCTBlobManager *blobManager = (__bridge RCTBlobManager *)_blobManagerLookup();
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
  // Resolve blob on current thread
  RCTBlobManager *blobManager = (__bridge RCTBlobManager *)_blobManagerLookup();
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

VideoFrameHandle ApplePlatformContext::loadVideoFrame(const std::string &path) {
  NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
  NSURL *url = [nsPath hasPrefix:@"file://"] ? [NSURL URLWithString:nsPath]
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
    (NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
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

std::function<void(void *)>
ApplePlatformContext::retainNativeWindow(void *window) {
  CFRetain(window);
  return [](void *layer) {
    // As MetalView's: the releaser can run on the rendering thread, and
    // CALayer teardown belongs on the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      CFRelease(layer);
    });
  };
}

// The NV12 variant of createTestVideoFrame: a biplanar BT.709 limited-range
// frame, red on the left half and green on the right, whose attachments
// declare its color space the way the decoder's frames do, so that the
// conversion of importExternalTexture is fully determined.
static VideoFrameHandle createTestYuvFrame(uint32_t width, uint32_t height) {
  NSDictionary *attrs = @{
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVPixelBufferRef pixelBuffer = NULL;
  CVReturn err =
      CVPixelBufferCreate(kCFAllocatorDefault, width, height,
                          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                          (__bridge CFDictionaryRef)attrs, &pixelBuffer);
  if (err != kCVReturnSuccess || !pixelBuffer) {
    throw std::runtime_error("createTestVideoFrame: unsupported: "
                             "CVPixelBufferCreate failed for a biplanar "
                             "YUV buffer");
  }
  CVBufferSetAttachment(pixelBuffer, kCVImageBufferYCbCrMatrixKey,
                        kCVImageBufferYCbCrMatrix_ITU_R_709_2,
                        kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(pixelBuffer, kCVImageBufferColorPrimariesKey,
                        kCVImageBufferColorPrimaries_ITU_R_709_2,
                        kCVAttachmentMode_ShouldPropagate);
  CVBufferSetAttachment(pixelBuffer, kCVImageBufferTransferFunctionKey,
                        kCVImageBufferTransferFunction_ITU_R_709_2,
                        kCVAttachmentMode_ShouldPropagate);

  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  uint8_t *luma = static_cast<uint8_t *>(
      CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
  size_t lumaRowBytes = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  for (uint32_t y = 0; y < height; ++y) {
    uint8_t *row = luma + y * lumaRowBytes;
    for (uint32_t x = 0; x < width; ++x) {
      row[x] = x < width / 2 ? test_yuv_frame::kRedY : test_yuv_frame::kGreenY;
    }
  }
  // The chroma plane is subsampled by two on both axes and interleaves Cb, Cr.
  uint8_t *chroma = static_cast<uint8_t *>(
      CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
  size_t chromaRowBytes = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
  size_t chromaWidth = CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
  size_t chromaHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer, 1);
  for (size_t y = 0; y < chromaHeight; ++y) {
    uint8_t *row = chroma + y * chromaRowBytes;
    for (size_t x = 0; x < chromaWidth; ++x) {
      bool red = x < chromaWidth / 2;
      row[x * 2 + 0] = red ? test_yuv_frame::kRedCb : test_yuv_frame::kGreenCb;
      row[x * 2 + 1] = red ? test_yuv_frame::kRedCr : test_yuv_frame::kGreenCr;
    }
  }
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

  try {
    auto handle = wrapCVPixelBuffer(pixelBuffer);
    CFRelease(pixelBuffer); // wrapCVPixelBuffer retained its own reference
    return handle;
  } catch (...) {
    CFRelease(pixelBuffer);
    throw;
  }
}

VideoFrameHandle
ApplePlatformContext::createTestVideoFrame(uint32_t width, uint32_t height,
                                           VideoPixelFormat format) {
  if (format == VideoPixelFormat::NV12) {
    return createTestYuvFrame(width, height);
  }
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
