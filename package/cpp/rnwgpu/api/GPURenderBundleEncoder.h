#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

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
  finish(std::optional<std::shared_ptr<GPURenderBundleDescriptor>> descriptor);
  void setBindGroup(
      uint32_t index,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
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

} // namespace rnwgpu