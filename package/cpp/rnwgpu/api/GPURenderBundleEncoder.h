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

#include "GPUBindGroup.h"
#include "GPURenderBundle.h"
#include "GPURenderBundleDescriptor.h"
#include "GPURenderPipeline.h"

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
  void setBindGroup(uint32_t groupIndex, std::shared_ptr<GPUBindGroup> group,
                    std::optional<std::vector<uint32_t>> dynamicOffsets);
  void setPipeline(std::shared_ptr<GPURenderPipeline> pipeline);
  void draw(uint32_t vertexCount, std::optional<uint32_t> instanceCount,
            std::optional<uint32_t> firstVertex,
            std::optional<uint32_t> firstInstance);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderBundleEncoder::getBrand, this);
    registerHybridMethod("finish", &GPURenderBundleEncoder::finish, this);
    registerHybridMethod("setBindGroup", &GPURenderBundleEncoder::setBindGroup,
                         this);
    registerHybridMethod("setPipeline", &GPURenderBundleEncoder::setPipeline,
                         this);
    registerHybridMethod("draw", &GPURenderBundleEncoder::draw, this);

    registerHybridGetter("label", &GPURenderBundleEncoder::getLabel, this);
  }

  inline const wgpu::RenderBundleEncoder get() { return _instance; }

private:
  wgpu::RenderBundleEncoder _instance;
  std::string _label;
};

// bool conv(wgpu::RenderBundleEncoder &out, const
// std::shared_ptr<GPURenderBundleEncoder> &in) {
//   out = in->get();
//   return true;
// }

} // namespace rnwgpu