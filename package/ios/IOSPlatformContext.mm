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


std::shared_ptr<ImageData> IOSPlatformContext::createImageBitmap(std::string blobId, double offset, double size) {
  RCTBlobManager* blobManager = [[RCTBridge currentBridge] moduleForClass:RCTBlobManager.class];
  NSData* blobData = [blobManager resolve:[NSString stringWithUTF8String:blobId.c_str()] offset:(long)offset size:(long)size];
    
  if (!blobData) {
      return nullptr;
  }
    
    UIImage* image = [UIImage imageWithData:blobData];
    if (!image) {
        return nullptr;
    }
    
    CGImageRef cgImage = image.CGImage;
    size_t width = CGImageGetWidth(cgImage);
    size_t height = CGImageGetHeight(cgImage);
    size_t bitsPerComponent = 8;
    size_t bytesPerRow = width * 4;
    
    std::vector<uint8_t> imageData(height * bytesPerRow);
    
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(imageData.data(), width, height, bitsPerComponent, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
    
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);
    
    auto result = std::make_shared<ImageData>();
    result->width = static_cast<int>(width);
    result->height = static_cast<int>(height);
    result->data = imageData.data();
    result->size = imageData.size();
    return result;
}

} // namespace rnwgpu
