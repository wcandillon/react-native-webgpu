#include "ApplePlatformContext.h"

#include <TargetConditionals.h>

#include <cstdlib>
#include <cstring>

#include <zlib.h>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <ImageIO/ImageIO.h>
#import <React/RCTBlobManager.h>
#import <React/RCTBridge+Private.h>
#import <ReactCommon/RCTTurboModule.h>

#include "AppleVideoPlayer.h"

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

// Decode an 8-bit, non-interlaced, truecolor (with or without alpha) PNG into
// straight (non-premultiplied) RGBA. Returns false for any other PNG variant
// or a non-PNG input, leaving `out` untouched so the caller can fall back to
// ImageIO.
//
// Apple's imaging stack (UIImage, CGImageSource, CGBitmapContext, Core Image)
// always premultiplies alpha at decode and quantizes to 8 bits, which destroys
// the original straight samples for low-alpha pixels. createImageBitmap's
// premultiplyAlpha "none" must return those samples intact, so PNGs (the only
// format that carries alpha here) are decoded directly instead.
static bool decodeStraightPng(const uint8_t *data, size_t size,
                              ImageData &out) {
  static const uint8_t kSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (size < 8 || memcmp(data, kSignature, 8) != 0) {
    return false;
  }

  auto readBE32 = [](const uint8_t *p) -> uint32_t {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
  };

  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t bitDepth = 0;
  uint8_t colorType = 0;
  uint8_t interlace = 0;
  bool haveHeader = false;
  std::vector<uint8_t> idat;

  size_t pos = 8;
  while (pos + 12 <= size) {
    uint32_t chunkLen = readBE32(data + pos);
    const uint8_t *type = data + pos + 4;
    const uint8_t *chunk = data + pos + 8;
    // Guard against a truncated / malformed chunk running past the buffer.
    if (chunkLen > size - pos - 12) {
      return false;
    }
    if (memcmp(type, "IHDR", 4) == 0 && chunkLen >= 13) {
      width = readBE32(chunk);
      height = readBE32(chunk + 4);
      bitDepth = chunk[8];
      colorType = chunk[9];
      interlace = chunk[12];
      haveHeader = true;
    } else if (memcmp(type, "IDAT", 4) == 0) {
      idat.insert(idat.end(), chunk, chunk + chunkLen);
    } else if (memcmp(type, "IEND", 4) == 0) {
      break;
    }
    pos += 12 + chunkLen;
  }

  int channels = 0;
  if (colorType == 2) {
    channels = 3; // truecolor RGB
  } else if (colorType == 6) {
    channels = 4; // truecolor RGBA
  }
  if (!haveHeader || bitDepth != 8 || interlace != 0 || channels == 0 ||
      width == 0 || height == 0 || idat.empty()) {
    return false;
  }

  const size_t rowBytes = static_cast<size_t>(width) * channels;
  const uLongf inflatedSize = (rowBytes + 1) * height; // +1 filter byte per row
  std::vector<uint8_t> inflated(inflatedSize);
  uLongf actualSize = inflatedSize;
  if (uncompress(inflated.data(), &actualSize, idat.data(),
                 static_cast<uLong>(idat.size())) != Z_OK ||
      actualSize != inflatedSize) {
    return false;
  }

  // Reverse the per-scanline PNG filters in place.
  std::vector<uint8_t> image(rowBytes * height);
  for (uint32_t y = 0; y < height; y++) {
    const uint8_t filter = inflated[y * (rowBytes + 1)];
    const uint8_t *src = &inflated[y * (rowBytes + 1) + 1];
    uint8_t *row = &image[y * rowBytes];
    const uint8_t *prev = y > 0 ? &image[(y - 1) * rowBytes] : nullptr;
    for (size_t i = 0; i < rowBytes; i++) {
      const int a = i >= static_cast<size_t>(channels) ? row[i - channels] : 0;
      const int b = prev ? prev[i] : 0;
      const int c =
          (prev && i >= static_cast<size_t>(channels)) ? prev[i - channels] : 0;
      int value = src[i];
      switch (filter) {
      case 0: // None
        break;
      case 1: // Sub
        value += a;
        break;
      case 2: // Up
        value += b;
        break;
      case 3: // Average
        value += (a + b) / 2;
        break;
      case 4: { // Paeth
        const int p = a + b - c;
        const int pa = std::abs(p - a);
        const int pb = std::abs(p - b);
        const int pc = std::abs(p - c);
        value += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
        break;
      }
      default:
        return false;
      }
      row[i] = static_cast<uint8_t>(value & 0xFF);
    }
  }

  out.width = width;
  out.height = height;
  out.format = wgpu::TextureFormat::RGBA8Unorm;
  out.premultiplied = false;
  out.data.resize(static_cast<size_t>(width) * height * 4);
  const size_t pixelCount = static_cast<size_t>(width) * height;
  for (size_t p = 0; p < pixelCount; p++) {
    const uint8_t *s = &image[p * channels];
    uint8_t *d = &out.data[p * 4];
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = channels == 4 ? s[3] : 255;
  }
  return true;
}

ImageData
ApplePlatformContext::createImageBitmapFromData(std::span<const uint8_t> data) {
  // PNGs carry alpha, and premultiplyAlpha "none" must preserve their straight
  // samples exactly, so decode them directly (Apple's imaging APIs premultiply
  // at decode). Everything else goes through ImageIO below.
  ImageData pngResult;
  if (decodeStraightPng(data.data(), data.size(), pngResult)) {
    return pngResult;
  }

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

  // Non-PNG sources (JPEG, ...) have no alpha channel, so the premultiplied
  // draw is exact for them; flag the result premultiplied so createImageBitmap
  // and copyExternalImageToTexture convert consistently.
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
