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

class GPUExternalTexture : public m::HybridObject {
public:
  explicit GPUExternalTexture(wgpu::ExternalTexture instance, std::string label)
      : HybridObject("GPUExternalTexture"), _instance(instance), _label(label) {
  }

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUExternalTexture::getBrand, this);

    registerHybridGetter("label", &GPUExternalTexture::getLabel, this);
  }

private:
  wgpu::ExternalTexture _instance;
  std::string _label;
};
} // namespace rnwgpu