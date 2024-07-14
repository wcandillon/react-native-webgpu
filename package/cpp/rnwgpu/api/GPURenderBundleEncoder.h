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

#include "GPURenderBundle.h"
#include "GPURenderBundleDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderBundleEncoder : public m::HybridObject {
public:
  explicit GPURenderBundleEncoder(wgpu::RenderBundleEncoder instance,
                                  std::string label)
      : HybridObject("GPURenderBundleEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPURenderBundle>
  finish(std::shared_ptr<GPURenderBundleDescriptor> descriptor);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundleEncoder::getBrand, this);
    registerHybridMethod("finish", &GPURenderBundleEncoder::finish, this);

    registerHybridGetter("label", &GPURenderBundleEncoder::getLabel, this);
  }

  inline const wgpu::RenderBundleEncoder get() { return _instance; }

private:
  wgpu::RenderBundleEncoder _instance;
  std::string _label;
};
} // namespace rnwgpu