#include "ApplePlatformContext.h"

#include <TargetConditionals.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <React/RCTBlobManager.h>
#import <React/RCTBridge+Private.h>
#import <ReactCommon/RCTTurboModule.h>

#include "RNWebGPUManager.h"
#include "WebGPUModule.h"

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

ApplePlatformContext::ApplePlatformContext() {
  checkIfUsingSimulatorWithAPIValidation();
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
  // This avoids a copy by assuming the UIImage/NSImage constructors
  // decode `nsData` eagerly before the memory for the wrapped `data`
  // is freed.
  //
  // Since we get the `CGImageRef` from `image` and then throw
  // it away, that's a fairly safe assumption.
  NSData *nsData =
      [NSData dataWithBytesNoCopy:const_cast<uint8_t *>(data.data())
                           length:data.size()
                     freeWhenDone:NO];

#if !TARGET_OS_OSX
  UIImage *image = [UIImage imageWithData:nsData];
#else
  NSImage *image = [[NSImage alloc] initWithData:nsData];
#endif
  if (!image) {
    throw std::runtime_error("Couldn't decode image");
  }

#if !TARGET_OS_OSX
  CGImageRef cgImage = image.CGImage;
#else
  CGImageRef cgImage = [image CGImageForProposedRect:NULL
                                             context:NULL
                                               hints:NULL];
#endif
  size_t width = CGImageGetWidth(cgImage);
  size_t height = CGImageGetHeight(cgImage);
  size_t bitsPerComponent = 8;
  size_t bytesPerRow = width * 4;

  ImageData result;
  result.width = static_cast<int>(width);
  result.height = static_cast<int>(height);
  result.data.resize(height * bytesPerRow);
  result.format = wgpu::TextureFormat::RGBA8Unorm;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      result.data.data(), width, height, bitsPerComponent, bytesPerRow,
      colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

  CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);

  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);

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
