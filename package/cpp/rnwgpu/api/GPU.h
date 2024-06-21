#pragma once

#include <string>
#include <future>

#include <RNFHybridObject.h>

#include "GPUAdapter.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU() : HybridObject("GPU") {}

public:
  std::shared_ptr<GPUAdapter> requestAdapter();

  std::string getBrand() { return _name; }

  void loadHybridMethods() override;
};
} // namespace rnwgpu
