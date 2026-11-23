#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <jsi/jsi.h>

#include "GPUSharedFenceState.h"
#include "JSIConverter.h"
#include "NativeFrameImport.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// The fence of a drawing passed as an argument: what
// GPUSharedTextureMemory.endAccess() returned ({ fences }), a list of fence
// states, or a single one, merged into one fence (see
// native_frame::mergeFences). `fence` stays null when there is none. Throws
// std::invalid_argument, its message starting with `caller` + "(): ", for a
// destroyed fence or fences that cannot be merged.
inline void fenceFromArgument(jsi::Runtime &runtime, const jsi::Value &value,
                              const char *caller, wgpu::SharedFence *fence,
                              uint64_t *signaledValue) {
  *signaledValue = 0;
  if (!value.isObject()) {
    return;
  }
  auto arg = value.getObject(runtime);
  jsi::Value list = jsi::Value::undefined();
  if (arg.isArray(runtime)) {
    list = jsi::Value(runtime, arg);
  } else if (arg.hasProperty(runtime, "fences")) {
    list = arg.getProperty(runtime, "fences");
  }
  std::vector<std::shared_ptr<GPUSharedFenceState>> states;
  if (list.isObject()) {
    states = JSIConverter<
        std::vector<std::shared_ptr<GPUSharedFenceState>>>::fromJSI(runtime,
                                                                    list,
                                                                    false);
  } else {
    states.push_back(
        JSIConverter<std::shared_ptr<GPUSharedFenceState>>::fromJSI(
            runtime, value, false));
  }
  std::vector<wgpu::SharedFence> fences;
  std::vector<uint64_t> values;
  wgpu::Device device;
  for (const auto &state : states) {
    if (!state || !state->fence) {
      continue;
    }
    auto native = state->fence->get();
    if (!native) {
      throw std::invalid_argument(std::string(caller) +
                                  "(): a fence was destroyed");
    }
    device = state->fence->device();
    fences.push_back(std::move(native));
    values.push_back(state->signaledValue);
  }
  try {
    native_frame::mergeFences(device, fences, values, fence, signaledValue,
                              caller);
  } catch (const std::exception &e) {
    throw std::invalid_argument(e.what());
  }
}

} // namespace rnwgpu
