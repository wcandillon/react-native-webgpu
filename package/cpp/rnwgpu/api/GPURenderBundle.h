#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"
#include "Convertors.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundle : public m::HybridObject {
public:
  explicit GPURenderBundle(wgpu::RenderBundle instance, std::string label)
      : HybridObject("GPURenderBundle"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundle::getBrand, this);

    registerHybridGetter("label", &GPURenderBundle::getLabel, this);
  }

  inline const wgpu::RenderBundle get() { return _instance; }

private:
  wgpu::RenderBundle _instance;
  std::string _label;
};

bool conv(wgpu::RenderBundle &out, const std::shared_ptr<GPURenderBundle> &in) {
  out = in->get();
  return true;
}

} // namespace rnwgpu