#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"
#include "PlatformContext.h"

namespace rnwgpu {

class ImageBitmap: public margelo::HybridObject {
public:
  ImageBitmap(ImageData &imageData): HybridObject("ImageBitmap"), _imageData(imageData) {}

  size_t getWidth() {
    return _imageData.width;
  }

  size_t getHeight() {
    return _imageData.height;
  }

    void loadHybridMethods() override {
      registerHybridGetter("width", &ImageBitmap::getWidth, this);
      registerHybridGetter("height", &ImageBitmap::getHeight, this);
    }

  private:
    ImageData _imageData;
};

} // namespace rnwgpu

