#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundle : public m::HybridObject {
public:
  explicit GPURenderBundle(std::shared_ptr<wgpu::RenderBundle> instance,
                           std::string label)
      : HybridObject("GPURenderBundle"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundle::getBrand, this);

    registerHybridGetter("label", &GPURenderBundle::getLabel, this);
  }

private:
  std::shared_ptr<wgpu::RenderBundle> _instance;
  std::string _label;
};
} // namespace rnwgpu