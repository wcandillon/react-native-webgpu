#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "NativeObject.h"
#include "PlatformContext.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class ImageBitmap : public NativeObject<ImageBitmap> {
public:
  static constexpr const char *CLASS_NAME = "ImageBitmap";

  explicit ImageBitmap(ImageData &imageData)
      : NativeObject(CLASS_NAME), _imageData(imageData) {}

  size_t getWidth() { return _imageData.width; }

  size_t getHeight() { return _imageData.height; }

  void *getData() { return _imageData.data.data(); }

  size_t getSize() { return _imageData.data.size(); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "width", &ImageBitmap::getWidth);
    installGetter(runtime, prototype, "height", &ImageBitmap::getHeight);
  }

  size_t getMemoryPressure() override { return getSize(); }

private:
  ImageData _imageData;
};

} // namespace rnwgpu
