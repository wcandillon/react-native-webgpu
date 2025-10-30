#pragma once

#include <algorithm>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCommandBuffer : public m::HybridObject {
public:
  explicit GPUCommandBuffer(wgpu::CommandBuffer instance, std::string label,
                            size_t estimatedBytes)
      : HybridObject("GPUCommandBuffer"), _instance(instance), _label(label),
        _estimatedBytes(std::max<size_t>(estimatedBytes, kBaseCommandBufferBytes)) {}

public:
  std::string getBrand() { return _name; }

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCommandBuffer::getBrand, this);

    registerHybridGetter("label", &GPUCommandBuffer::getLabel, this);
    registerHybridSetter("label", &GPUCommandBuffer::setLabel, this);
  }

  inline const wgpu::CommandBuffer get() { return _instance; }

  size_t getMemoryPressure() override { return _estimatedBytes; }

private:
  static constexpr size_t kBaseCommandBufferBytes = 8 * 1024;
  wgpu::CommandBuffer _instance;
  std::string _label;
  size_t _estimatedBytes;
};

} // namespace rnwgpu
