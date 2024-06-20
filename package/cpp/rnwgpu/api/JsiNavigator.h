#pragma once

#include <RNFHybridObject.h>

namespace rnwgpu {

  namespace m = margelo;

  class Navigator: public m::HybridObject {
    public:
      explicit Navigator(): HybridObject("Navigator") {}

    public:
      double getGPU() {
        return 1.0;
      }

      void loadHybridMethods() override;
  };
} // namespace rnwgpu