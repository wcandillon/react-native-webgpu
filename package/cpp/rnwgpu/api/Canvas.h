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
  explicit Canvas(uint64_t surface, const float width, const float height)
      : HybridObject("Canvas"), _surface(surface), _width(width),
        _height(height) {}

  float getWidth() { return _width; }
  float getHeight() { return _height; }

  void setWidth(const float width) { _width = width; }
  void setHeight(const float height) { _height = height; }

  float getClientWidth() { return _width; }
  float getClientHeight() { return _height; }

  uint64_t getSurface() { return _surface; }

  void loadHybridMethods() override {
    registerHybridGetter("surface", &Canvas::getSurface, this);
    registerHybridGetter("width", &Canvas::getWidth, this);
    registerHybridGetter("height", &Canvas::getHeight, this);
    registerHybridGetter("clientWidth", &Canvas::getClientWidth, this);
    registerHybridGetter("clientHeight", &Canvas::getClientHeight, this);
    registerHybridSetter("width", &Canvas::setWidth, this);
    registerHybridSetter("height", &Canvas::setHeight, this);
  }

private:
  uint64_t _surface;
  float _width;
  float _height;
};

} // namespace rnwgpu
