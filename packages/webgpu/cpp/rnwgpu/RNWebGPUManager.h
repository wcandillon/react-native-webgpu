#pragma once

#include <memory>

#include "GPU.h"
#include "PlatformContext.h"
#include "SurfaceRegistry.h"

namespace facebook {
namespace jsi {
class Runtime;
} // namespace jsi
namespace react {
class CallInvoker;
}
} // namespace facebook

namespace rnwgpu {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

class RNWebGPUManager {
public:
  RNWebGPUManager(jsi::Runtime *jsRuntime,
                  std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
                  std::shared_ptr<PlatformContext> platformContext);
  ~RNWebGPUManager();

private:
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;

public:
  wgpu::Instance _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

} // namespace rnwgpu
