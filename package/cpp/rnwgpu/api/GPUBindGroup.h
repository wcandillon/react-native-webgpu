#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBindGroup : public m::HybridObject {
public:
  explicit GPUBindGroup(wgpu::BindGroup instance, std::string label)
      : HybridObject("GPUBindGroup"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUBindGroup::getBrand, this);

    registerHybridGetter("label", &GPUBindGroup::getLabel, this);
  }

  inline const wgpu::BindGroup get() { return _instance; }

private:
  wgpu::BindGroup _instance;
  std::string _label;
};

// bool conv(wgpu::BindGroup &out, const std::shared_ptr<GPUBindGroup> &in) {
//   out = in->get();
//   return true;
// }

} // namespace rnwgpu