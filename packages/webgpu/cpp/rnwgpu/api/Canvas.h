#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "NativeObject.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class Canvas : public NativeObject<Canvas> {
public:
  static constexpr const char *CLASS_NAME = "Canvas";

  explicit Canvas(void *surface, const int width, const int height,
                  std::shared_ptr<void> surfaceOwner = {})
      : NativeObject(CLASS_NAME), _surfaceOwner(std::move(surfaceOwner)),
        _surface(surface), _width(width), _height(height), _clientWidth(width),
        _clientHeight(height) {}

  int getWidth() { return _width; }
  int getHeight() { return _height; }

  void setWidth(const int width) { _width = width; }
  void setHeight(const int height) { _height = height; }

  int getClientWidth() { return _clientWidth; }
  int getClientHeight() { return _clientHeight; }

  void setClientWidth(const int width) { _clientWidth = width; }

  void setClientHeight(const int height) { _clientHeight = height; }

  void *getSurface() { return _surface; }

  // No-op DOM-compatibility stubs so that web renderers (Three.js,
  // react-three-fiber) can treat the canvas like an HTMLCanvasElement without
  // needing a JS wrapper. Extra JS arguments are ignored.
  void addEventListener() {}
  void removeEventListener() {}
  void dispatchEvent() {}
  void setPointerCapture() {}
  void releasePointerCapture() {}

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "surface", &Canvas::getSurface);
    installGetterSetter(runtime, prototype, "width", &Canvas::getWidth,
                        &Canvas::setWidth);
    installGetterSetter(runtime, prototype, "height", &Canvas::getHeight,
                        &Canvas::setHeight);
    installGetter(runtime, prototype, "clientWidth", &Canvas::getClientWidth);
    installGetter(runtime, prototype, "clientHeight", &Canvas::getClientHeight);
    installMethod(runtime, prototype, "addEventListener",
                  &Canvas::addEventListener);
    installMethod(runtime, prototype, "removeEventListener",
                  &Canvas::removeEventListener);
    installMethod(runtime, prototype, "dispatchEvent", &Canvas::dispatchEvent);
    installMethod(runtime, prototype, "setPointerCapture",
                  &Canvas::setPointerCapture);
    installMethod(runtime, prototype, "releasePointerCapture",
                  &Canvas::releasePointerCapture);
  }

private:
  // Keeps platform-owned handles (for example ANativeWindow) alive for as
  // long as JavaScript can observe the raw surface pointer.
  std::shared_ptr<void> _surfaceOwner;
  void *_surface;
  int _width;
  int _height;
  int _clientWidth;
  int _clientHeight;
};

} // namespace rnwgpu
