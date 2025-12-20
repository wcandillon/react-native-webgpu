#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

namespace rnwgpu {

namespace m = margelo;

class Canvas : public m::HybridObject {
public:
  explicit Canvas(void *surface, const int width, const int height)
      : HybridObject("Canvas"), _surface(surface), _width(width),
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

  void loadHybridMethods() override {
    registerHybridGetter("surface", &Canvas::getSurface, this);
    registerHybridGetter("width", &Canvas::getWidth, this);
    registerHybridGetter("height", &Canvas::getHeight, this);
    registerHybridGetter("clientWidth", &Canvas::getClientWidth, this);
    registerHybridGetter("clientHeight", &Canvas::getClientHeight, this);
    registerHybridSetter("width", &Canvas::setWidth, this);
    registerHybridSetter("height", &Canvas::setHeight, this);
  }

  size_t getMemoryPressure() override { return sizeof(Canvas); }

private:
  void *_surface;
  int _width;
  int _height;
  int _clientWidth;
  int _clientHeight;
};

} // namespace rnwgpu
