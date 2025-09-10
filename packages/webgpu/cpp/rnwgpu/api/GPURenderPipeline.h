#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayout.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderPipeline : public m::HybridObject {
public:
  explicit GPURenderPipeline(wgpu::RenderPipeline instance, std::string label)
      : HybridObject("GPURenderPipeline"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPUBindGroupLayout> getBindGroupLayout(uint32_t index);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPipeline::getBrand, this);
    registerHybridMethod("getBindGroupLayout",
                         &GPURenderPipeline::getBindGroupLayout, this);

    registerHybridGetter("label", &GPURenderPipeline::getLabel, this);
    registerHybridSetter("label", &GPURenderPipeline::setLabel, this);
  }

  inline const wgpu::RenderPipeline get() { return _instance; }

  size_t getMemoryPressure() override {
    // Render pipelines contain compiled shader state, vertex/fragment shaders,
    // render state, and driver-specific optimized code
    // Estimate: 24KB for a typical render pipeline with vertex + fragment
    // shaders
    return 24 * 1024;
  }

private:
  wgpu::RenderPipeline _instance;
  std::string _label;
  friend class GPUDevice;
};

} // namespace rnwgpu