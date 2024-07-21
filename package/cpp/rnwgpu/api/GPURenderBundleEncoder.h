#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPURenderBundle.h"
#include "GPURenderBundleDescriptor.h"
#include "GPURenderPipeline.h"
#include "variant.h"
#include "vector.h"

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
  finish(std::optional<std::shared_ptr<GPURenderBundleDescriptor>> descriptor);
  void setBindGroup(
      double index,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
      std::optional<std::vector<double>> dynamicOffsets);
  void setPipeline(std::shared_ptr<GPURenderPipeline> pipeline);
  void draw(double vertexCount, std::optional<double> instanceCount,
            std::optional<double> firstVertex,
            std::optional<double> firstInstance);

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

} // namespace rnwgpu