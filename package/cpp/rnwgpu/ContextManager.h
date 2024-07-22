#pragma once

#include <memory>
#include <jsi/jsi.h>
#include "RNFHybridObject.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;
namespace react = facebook::react;
namespace m = margelo;

class ContextManager : public m::HybridObject {
public:
  ContextManager(const char *name);
};

} // namespace rnwgpu
