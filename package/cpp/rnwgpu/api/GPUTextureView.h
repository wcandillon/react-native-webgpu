#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTextureView : public m::HybridObject {
public:
  explicit GPUTextureView(wgpu::TextureView instance, std::string label)
      : HybridObject("GPUTextureView"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUTextureView::getBrand, this);

    registerHybridGetter("label", &GPUTextureView::getLabel, this);
  }

  // private:
  wgpu::TextureView _instance;
  std::string _label;
};
} // namespace rnwgpu