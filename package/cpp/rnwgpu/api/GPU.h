#pragma once

#include <future>
#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"
#include <RNFHybridObject.h>

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU();

public:
  std::future<std::shared_ptr<GPUAdapter>>
  requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options);

  std::string getBrand() { return _name; }

  void loadHybridMethods() override;

private:
  wgpu::Instance _instance;
};
} // namespace rnwgpu
