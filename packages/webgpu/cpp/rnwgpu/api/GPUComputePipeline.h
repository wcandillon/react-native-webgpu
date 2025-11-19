#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayout.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePipeline : public m::HybridObject {
public:
  explicit GPUComputePipeline(wgpu::ComputePipeline instance, std::string label)
      : HybridObject("GPUComputePipeline"), _instance(instance), _label(label) {
  }

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPUBindGroupLayout> getBindGroupLayout(uint32_t index);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePipeline::getBrand, this);
    registerHybridMethod("getBindGroupLayout",
                         &GPUComputePipeline::getBindGroupLayout, this);

    registerHybridGetter("label", &GPUComputePipeline::getLabel, this);
    registerHybridSetter("label", &GPUComputePipeline::setLabel, this);
  }

  inline const wgpu::ComputePipeline get() { return _instance; }

  size_t getMemoryPressure() override {
    // Compute pipelines retain compiled shader state and backend caches.
    // Overshoot intentionally to reflect the native resources they fan out to.
    return 3 * 1024 * 1024; // 3MB
  }

private:
  wgpu::ComputePipeline _instance;
  std::string _label;
  friend class GPUDevice;
};

} // namespace rnwgpu
