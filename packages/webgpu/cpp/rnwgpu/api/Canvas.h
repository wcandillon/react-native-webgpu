#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "NativeObject.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class Canvas : public NativeObject<Canvas> {
public:
  static constexpr const char *CLASS_NAME = "Canvas";

  explicit Canvas(void *surface, const float width, const float height,
    const float pixelRatio)
      : NativeObject(CLASS_NAME), _surface(surface), _width(width * pixelRatio),
        _height(height * pixelRatio), _clientWidth(width), _clientHeight(height) {}

  float getWidth() { return _width; }
  float getHeight() { return _height; }

  void setWidth(const int width) { _width = width; }
  void setHeight(const int height) { _height = height; }

  float getClientWidth() { return _clientWidth; }
  float getClientHeight() { return _clientHeight; }

  void setClientWidth(const float width) { _clientWidth = width; }

  void setClientHeight(const float height) { _clientHeight = height; }

  void *getSurface() { return _surface; }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "surface", &Canvas::getSurface);
    installGetterSetter(runtime, prototype, "width", &Canvas::getWidth,
                        &Canvas::setWidth);
    installGetterSetter(runtime, prototype, "height", &Canvas::getHeight,
                        &Canvas::setHeight);
    installGetterSetter(runtime, prototype, "clientWidth", &Canvas::getClientWidth,
                        &Canvas::setClientWidth);
    installGetterSetter(runtime, prototype, "clientHeight", &Canvas::getClientHeight,
                        &Canvas::setClientHeight);
  }

private:
  void *_surface;
  float _width;
  float _height;
  float _clientWidth;
  float _clientHeight;
};

} // namespace rnwgpu
