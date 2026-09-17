#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "NativeObject.h"
#include "PlatformContext.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// Convert RGBA8 pixel data in place between straight and premultiplied alpha.
// Uses the same integer rounding as the web reference polyfill in the test
// harness (setup.ts, convertAlpha) so the native result is bit-exact with the
// dawn.node client. A no-op when the source and destination representations
// already match.
inline void convertAlpha(uint8_t *data, size_t byteLength,
                         bool sourcePremultiplied,
                         bool destinationPremultiplied) {
  if (sourcePremultiplied == destinationPremultiplied) {
    return;
  }
  for (size_t i = 0; i + 3 < byteLength; i += 4) {
    const uint32_t alpha = data[i + 3];
    for (size_t channel = 0; channel < 3; channel++) {
      const uint32_t value = data[i + channel];
      if (destinationPremultiplied) {
        data[i + channel] = static_cast<uint8_t>((value * alpha + 127) / 255);
      } else if (alpha == 0) {
        data[i + channel] = 0;
      } else {
        const uint32_t straight = (value * 255 + (alpha >> 1)) / alpha;
        data[i + channel] = static_cast<uint8_t>(straight > 255 ? 255 : straight);
      }
    }
  }
}

class ImageBitmap : public NativeObject<ImageBitmap> {
public:
  static constexpr const char *CLASS_NAME = "ImageBitmap";

  explicit ImageBitmap(ImageData &imageData)
      : NativeObject(CLASS_NAME), _imageData(imageData) {}

  size_t getWidth() { return _imageData.width; }

  size_t getHeight() { return _imageData.height; }

  void *getData() { return _imageData.data.data(); }

  size_t getSize() { return _imageData.data.size(); }

  // Whether the stored pixels are premultiplied by alpha. Used by
  // copyExternalImageToTexture to decide whether a conversion to the
  // destination's premultipliedAlpha representation is needed.
  bool getPremultiplied() { return _imageData.premultiplied; }

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
