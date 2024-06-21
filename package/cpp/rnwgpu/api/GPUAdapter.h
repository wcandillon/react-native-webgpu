#pragma once

#include <RNFHybridObject.h>
#include <string>

namespace rnwgpu {

namespace m = margelo;

class GPUAdapter : public m::HybridObject {
public:
  GPUAdapter(): HybridObject("GPUAdapter") {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override;
};
} // namespace rnwgpu