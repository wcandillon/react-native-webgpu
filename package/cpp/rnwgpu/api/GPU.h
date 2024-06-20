#pragma once

#include <RNFHybridObject.h>
#include <string>

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU() : HybridObject("GPU") {}

public:
  double getGPU() { return 1.0; }
  std::string getBrand() { return _name; }

  void loadHybridMethods() override;
};
} // namespace rnwgpu