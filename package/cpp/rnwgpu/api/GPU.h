#pragma once

#include <future>
#include <memory>
#include <string>

#include <RNFHybridObject.h>

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU() : HybridObject("GPU") {}

public:
  std::future<std::shared_ptr<GPUAdapter>>
  requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options);

  std::string getBrand() { return _name; }

  void loadHybridMethods() override;
};
} // namespace rnwgpu
