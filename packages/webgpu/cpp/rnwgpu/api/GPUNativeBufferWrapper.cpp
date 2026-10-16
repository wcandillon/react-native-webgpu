#include "GPUNativeBufferWrapper.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "DestroyedError.h"
#include "GPUSharedFence.h"
#include "GPUSharedFenceState.h"
#include "NativeFrameImport.h"

namespace rnwgpu {

NativeBufferAccess::NativeBufferAccess(const char *className,
                                       wgpu::Device device,
                                       wgpu::SharedTextureMemory memory,
                                       wgpu::Texture texture,
                                       std::shared_ptr<VideoFrame> source,
                                       std::string label)
    : _className(className), _device(std::move(device)),
      _memory(std::move(memory)), _texture(std::move(texture)),
      _source(std::move(source)), _label(std::move(label)) {}

// The garbage-collection fallback: releases without reporting, there is no
// one left to report to.
NativeBufferAccess::~NativeBufferAccess() { release(false); }

void NativeBufferAccess::setLabel(const std::string &label) {
  _label = label;
  if (_memory) {
    _memory.SetLabel(_label.c_str());
  }
}

void NativeBufferAccess::injectDestroyedError(const char *method) {
  reportDestroyedUse(_device, _className, _label, method);
}

void NativeBufferAccess::beginAccess(jsi::Runtime &runtime,
                                     const jsi::Value *args, size_t count) {
  const std::string caller = std::string(_className) + "::beginAccess";
  // Fails like an access Dawn rejects: a validation error, then the throw.
  if (!_memory) {
    injectDestroyedError("beginAccess");
    throw jsi::JSError(runtime, caller + "(): the wrapper was destroyed");
  }
  // Built in lockstep so fenceCount covers both arrays, and kept in locals so
  // the raw pointers outlive the synchronous BeginAccess() below.
  std::vector<wgpu::SharedFence> fences;
  std::vector<uint64_t> signaledValues;
  if (count > 0 && !args[0].isUndefined() && !args[0].isNull()) {
    auto states = JSIConverter<
        std::vector<std::shared_ptr<GPUSharedFenceState>>>::fromJSI(runtime,
                                                                    args[0],
                                                                    false);
    for (const auto &state : states) {
      if (!state || !state->fence) {
        continue;
      }
      auto fence = state->fence->get();
      if (!fence) {
        state->fence->injectDestroyedError("beginAccess");
        throw jsi::JSError(runtime, caller + "(): a fence was destroyed");
      }
      fences.push_back(std::move(fence));
      signaledValues.push_back(state->signaledValue);
    }
  }
  try {
    native_frame::beginAccess(_memory, _texture, true, fences, signaledValues,
                              caller.c_str());
  } catch (const std::exception &e) {
    throw jsi::JSError(runtime, e.what());
  }
  _accessOpen = true;
}

jsi::Value NativeBufferAccess::endAccess(jsi::Runtime &runtime) {
  const std::string caller = std::string(_className) + "::endAccess";
  if (!_memory) {
    injectDestroyedError("endAccess");
    throw jsi::JSError(runtime, caller + "(): the wrapper was destroyed");
  }
  wgpu::SharedTextureMemoryEndAccessState state{};
  try {
    native_frame::endAccess(_memory, _texture, &state, caller.c_str());
  } catch (const std::exception &e) {
    throw jsi::JSError(runtime, e.what());
  }
  _accessOpen = false;

  wgpu::SharedFence fence;
  uint64_t signaledValue = 0;
  bool hasFence = false;
  try {
    hasFence = native_frame::mergeFences(_device, state, &fence, &signaledValue,
                                         caller.c_str());
  } catch (const std::exception &e) {
    throw jsi::JSError(runtime, e.what());
  }
  if (!hasFence) {
    return jsi::Value::null();
  }
  jsi::Object result(runtime);
  result.setProperty(
      runtime, "fence",
      GPUSharedFence::create(runtime, std::make_shared<GPUSharedFence>(
                                          _device, std::move(fence), "")));
  result.setProperty(runtime, "signaledValue",
                     jsi::BigInt::fromUint64(runtime, signaledValue));
  return result;
}

void NativeBufferAccess::destroy() { release(true); }

void NativeBufferAccess::release(bool reportOpenAccess) {
  if (!_memory) {
    return;
  }
  if (_accessOpen) {
    if (reportOpenAccess) {
      // Dawn's contract is an endAccess() for every beginAccess(). Report the
      // misuse, and end the access anyway: the wrapper owns the texture, so
      // an access left open would keep the buffer from its producer for good.
      auto message = "Destroyed " + describeDestroyed(_className, _label) +
                     " with an access still open: call endAccess() first. The "
                     "access was ended; its fence is lost.";
      _device.InjectError(wgpu::ErrorType::Validation, message.c_str());
    }
    native_frame::endAccess(_memory, _texture);
    _accessOpen = false;
  }
  if (_texture) {
    _texture.Destroy();
  }
  _texture = nullptr;
  _memory = nullptr;
  _source = nullptr;
}

} // namespace rnwgpu
