#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTexture : public m::HybridObject {
public:
  explicit GPUTexture(wgpu::Texture instance, std::string label)
      : HybridObject("GPUTexture"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUTexture::getBrand, this);

    registerHybridGetter("label", &GPUTexture::getLabel, this);
  }

  inline const wgpu::Texture get() { return _instance; }

private:
  wgpu::Texture _instance;
  std::string _label;
};
} // namespace rnwgpu