#include "IOSPlatformContext.h"

#import <React/RCTBlobManager.h>
#import <React/RCTBridge+Private.h>
#import <ReactCommon/RCTTurboModule.h>

#include "RNWebGPUManager.h"
#include "WebGPUModule.h"

namespace rnwgpu {

wgpu::Surface IOSPlatformContext::makeSurface(wgpu::Instance instance,
                                              void *surface, int width,
                                              int height) {
  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = surface;
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  return instance.CreateSurface(&surfaceDescriptor);
}

ImageData IOSPlatformContext::createImageBitmap(std::string blobId,
                                                double offset, double size) {
  RCTBlobManager *blobManager =
      [[RCTBridge currentBridge] moduleForClass:RCTBlobManager.class];
  NSData *blobData =
      [blobManager resolve:[NSString stringWithUTF8String:blobId.c_str()]
                    offset:(long)offset
                      size:(long)size];

  if (!blobData) {
    throw std::runtime_error("Couldn't retrive blob data");
  }

  UIImage *image = [UIImage imageWithData:blobData];
  if (!image) {
    throw std::runtime_error("Couldn't decode image");
  }

  CGImageRef cgImage = image.CGImage;
  size_t width = CGImageGetWidth(cgImage);
  size_t height = CGImageGetHeight(cgImage);
  size_t bitsPerComponent = 8;
  size_t bytesPerRow = width * 4;
  std::vector<uint8_t> imageData(height * bytesPerRow);

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      imageData.data(), width, height, bitsPerComponent, bytesPerRow,
      colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

  CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);

  // Now imageData contains a copy of the bitmap data

  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);

  // Use the copied data
  ImageData result;
  result.width = static_cast<int>(width);
  result.height = static_cast<int>(height);
  result.data = imageData;
  result.format = wgpu::TextureFormat::RGBA8Unorm;
  return result;
}

} // namespace rnwgpu
