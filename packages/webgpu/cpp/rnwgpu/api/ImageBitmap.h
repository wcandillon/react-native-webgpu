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

  ImageBitmap(ImageData &imageData, bool premultipliedAlpha)
      : NativeObject(CLASS_NAME), _imageData(imageData) {
    convertAlpha(_imageData.data, _imageData.premultipliedAlpha,
                 premultipliedAlpha);
    _imageData.premultipliedAlpha = premultipliedAlpha;
  }

  size_t getWidth() { return _imageData.width; }

  size_t getHeight() { return _imageData.height; }

  void *getData() { return _imageData.data.data(); }

  size_t getSize() { return _imageData.data.size(); }

  bool isPremultiplied() { return _imageData.premultipliedAlpha; }

  static void convertAlpha(std::vector<uint8_t> &data,
                           bool sourcePremultipliedAlpha,
                           bool destinationPremultipliedAlpha) {
    if (sourcePremultipliedAlpha == destinationPremultipliedAlpha) {
      return;
    }

    for (size_t i = 0; i + 3 < data.size(); i += 4) {
      uint32_t alpha = data[i + 3];
      for (size_t channel = 0; channel < 3; ++channel) {
        uint32_t value = data[i + channel];
        if (destinationPremultipliedAlpha) {
          value = (value * alpha + 127) / 255;
        } else if (alpha == 0) {
          value = 0;
        } else {
          value = (value * 255 + alpha / 2) / alpha;
          value = value > 255 ? 255 : value;
        }
        data[i + channel] = static_cast<uint8_t>(value);
      }
    }
  }

  void close() {
    _imageData.data.clear();
    _imageData.data.shrink_to_fit();
    _imageData.width = 0;
    _imageData.height = 0;
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "width", &ImageBitmap::getWidth);
    installGetter(runtime, prototype, "height", &ImageBitmap::getHeight);
    installMethod(runtime, prototype, "close", &ImageBitmap::close);
  }

  size_t getMemoryPressure() override { return getSize(); }

private:
  ImageData _imageData;
};

} // namespace rnwgpu
