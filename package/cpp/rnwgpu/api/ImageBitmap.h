#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"
#include "RNFHybridObject.h"

namespace rnwgpu {

class ImageBitmap : public margelo::HybridObject {
public:
  explicit ImageBitmap(ImageData &imageData)
      : HybridObject("ImageBitmap"), _imageData(imageData) {}

  size_t getWidth() { return _imageData.width; }

  size_t getHeight() { return _imageData.height; }

  void *getData() { return _imageData.data.data(); }

  size_t getSize() { return _imageData.data.size(); }

  void loadHybridMethods() override {
    registerHybridGetter("width", &ImageBitmap::getWidth, this);
    registerHybridGetter("height", &ImageBitmap::getHeight, this);
  }

private:
  ImageData _imageData;
};

} // namespace rnwgpu
