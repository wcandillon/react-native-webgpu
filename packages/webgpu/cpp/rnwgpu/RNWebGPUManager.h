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

  /**
   * Install global helper functions for Worklets serialization.
   * This installs __webgpuIsWebGPUObject and __webgpuBox on the global object.
   * Can be called on any runtime (main JS, UI, or custom worklet runtimes).
   */
  static void installWebGPUWorkletHelpers(jsi::Runtime &runtime);

  /**
   * Applies a surface attach latched by the platform UI thread (see
   * SurfaceInfo::applyPendingAttach). Apple runs the transition on main;
   * other platforms use the JS CallInvoker. This covers contexts that are not
   * actively rendering, so the last offscreen frame still reaches the screen.
   */
  void flushPendingSurfaceTransition(std::shared_ptr<SurfaceInfo> info);

private:
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;

public:
  wgpu::Instance _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

} // namespace rnwgpu
