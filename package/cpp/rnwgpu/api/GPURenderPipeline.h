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

#include "GPUBindGroupLayout.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderPipeline : public m::HybridObject {
public:
  explicit GPURenderPipeline(wgpu::RenderPipeline instance, std::string label)
      : HybridObject("GPURenderPipeline"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPUBindGroupLayout> getBindGroupLayout(uint32_t groupIndex);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPipeline::getBrand, this);
    registerHybridMethod("getBindGroupLayout",
                         &GPURenderPipeline::getBindGroupLayout, this);

    registerHybridGetter("label", &GPURenderPipeline::getLabel, this);
  }

  inline const wgpu::RenderPipeline get() { return _instance; }

private:
  wgpu::RenderPipeline _instance;
  std::string _label;
};

// bool conv(wgpu::RenderPipeline &out, const std::shared_ptr<GPURenderPipeline>
// &in) {
//   out = in->get();
//   return true;
// }

} // namespace rnwgpu