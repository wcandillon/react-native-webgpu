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

class GPUSampler : public m::HybridObject {
public:
  explicit GPUSampler(wgpu::Sampler instance, std::string label)
      : HybridObject("GPUSampler"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUSampler::getBrand, this);

    registerHybridGetter("label", &GPUSampler::getLabel, this);
  }

  inline const wgpu::Sampler get() { return _instance; }

private:
  wgpu::Sampler _instance;
  std::string _label;
};

// bool conv(wgpu::Sampler &out, const std::shared_ptr<GPUSampler> &in) {
//   out = in->get();
//   return true;
// }

} // namespace rnwgpu