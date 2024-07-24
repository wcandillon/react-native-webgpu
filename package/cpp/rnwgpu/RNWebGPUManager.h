#pragma once

#include <memory>
#include "GPU.h"
#include "SurfaceRegistry.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

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
                  std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker);
  ~RNWebGPUManager();

  SurfaceRegistry surfacesRegistry;

  std::shared_ptr<GPU> getGPU();

private:
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;
  std::shared_ptr<GPU> _gpu;
};

} // namespace rnwgpu
