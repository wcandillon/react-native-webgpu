#include "ApplePlatformContext.h"

#include <TargetConditionals.h>

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

ImageData ApplePlatformContext::createImageBitmapFromData(
    std::span<const uint8_t> data) {
  // This avoids a copy by assuming the UIImage/NSImage constructors
  // decode `nsData` eagerly before the memory for the wrapped `data`
  // is freed.
  //
  // Since we get the `CGImageRef` from `image` and then throw
  // it away, that's a fairly safe assumption.
  NSData *nsData = [NSData dataWithBytesNoCopy:const_cast<uint8_t *>(data.data())
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

  dispatch_async(
      dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
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

} // namespace rnwgpu
