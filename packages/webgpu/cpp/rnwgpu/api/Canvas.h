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
  explicit Canvas(void* surface, const float width, const float height, const int contextId)
      : HybridObject("Canvas"), _surface(surface), _width(width),
        _height(height), _clientWidth(width), _clientHeight(height), _contextId(contextId) {}

  float getWidth() { return _width; }
  float getHeight() { return _height; }

  void setWidth(const float width) { _width = width; }
  void setHeight(const float height) { _height = height; }

  float getClientWidth() { return _clientWidth; }
  float getClientHeight() { return _clientHeight; }

  void setClientWidth(const float width) { _clientWidth = width; }

  void setClientHeight(const float height) { _clientHeight = height; }

  void* getSurface() { return _surface; }
  void setSurface(void *surface) { _surface = surface; }
  int getContextId() { return _contextId; }

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
  void* _surface;
  float _width;
  float _height;
  float _clientWidth;
  float _clientHeight;
  int _contextId;
};

} // namespace rnwgpu
