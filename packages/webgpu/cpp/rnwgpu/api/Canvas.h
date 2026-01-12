#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFNativeObject.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class Canvas : public NativeObject<Canvas> {
public:
  static constexpr const char *CLASS_NAME = "Canvas";

  explicit Canvas(void *surface, const int width, const int height)
      : NativeObject(CLASS_NAME), _surface(surface), _width(width),
        _height(height), _clientWidth(width), _clientHeight(height) {}

  int getWidth() { return _width; }
  int getHeight() { return _height; }

  void setWidth(const int width) { _width = width; }
  void setHeight(const int height) { _height = height; }

  int getClientWidth() { return _clientWidth; }
  int getClientHeight() { return _clientHeight; }

  void setClientWidth(const int width) { _clientWidth = width; }

  void setClientHeight(const int height) { _clientHeight = height; }

  void *getSurface() { return _surface; }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "surface", &Canvas::getSurface);
    installGetterSetter(runtime, prototype, "width", &Canvas::getWidth,
                        &Canvas::setWidth);
    installGetterSetter(runtime, prototype, "height", &Canvas::getHeight,
                        &Canvas::setHeight);
    installGetter(runtime, prototype, "clientWidth", &Canvas::getClientWidth);
    installGetter(runtime, prototype, "clientHeight", &Canvas::getClientHeight);
  }

private:
  void *_surface;
  int _width;
  int _height;
  int _clientWidth;
  int _clientHeight;
};

} // namespace rnwgpu
