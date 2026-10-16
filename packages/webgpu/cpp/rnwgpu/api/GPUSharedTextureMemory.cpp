#include "GPUSharedTextureMemory.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Convertors.h"
#include "DestroyedError.h"
#include "NativeFrameImport.h"

namespace rnwgpu {

void GPUSharedTextureMemory::destroy() {
  if (!_instance) {
    return;
  }
  (void)_instance.GetProperties(&_properties);
  if (!_accessed.empty()) {
    // Dawn's contract is an endAccess() for every beginAccess(), but Dawn
    // doesn't check it when the memory goes away. Report the misuse; the
    // access itself is left as is (ending it would hand its fences to no one).
    auto message = "Destroyed " + describeDestroyed(CLASS_NAME, _label) +
                   " with an access still open: call endAccess() first.";
    _device.InjectError(wgpu::ErrorType::Validation, message.c_str());
    _accessed.clear();
  }
  _instance = nullptr;
}

void GPUSharedTextureMemory::injectDestroyedError(const char *method) {
  reportDestroyedUse(_device, CLASS_NAME, _label, method);
}

std::shared_ptr<GPUTexture> GPUSharedTextureMemory::createTexture(
    std::optional<std::shared_ptr<GPUTextureDescriptor>> descriptor) {
  if (!_instance) {
    // Like a texture Dawn can't create from the memory: the validation error
    // says why, and the texture is an error texture. As Dawn does, it is
    // described by the caller's descriptor, or else by the memory's
    // properties. It owns no GPU memory, so it reports no memory pressure.
    injectDestroyedError("createTexture");
    wgpu::TextureDescriptor desc{};
    std::string label;
    if (descriptor.has_value() && descriptor.value() != nullptr) {
      Convertor conv;
      if (!conv(desc, descriptor.value())) {
        throw std::runtime_error(
            "GPUSharedTextureMemory::createTexture(): Error with "
            "GPUTextureDescriptor");
      }
      label = descriptor.value()->label.value_or("");
    } else {
      desc.usage = _properties.usage;
      desc.size = _properties.size;
      desc.format = _properties.format;
    }
    return std::make_shared<GPUTexture>(_device.CreateErrorTexture(&desc),
                                        std::move(label), false);
  }
  if (!descriptor.has_value() || descriptor.value() == nullptr) {
    auto texture = _instance.CreateTexture();
    return std::make_shared<GPUTexture>(texture, "");
  }

  wgpu::TextureDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor.value())) {
    throw std::runtime_error(
        "GPUSharedTextureMemory::createTexture(): Error with "
        "GPUTextureDescriptor");
  }
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture,
                                      descriptor.value()->label.value_or(""));
}

void GPUSharedTextureMemory::beginAccess(
    std::shared_ptr<GPUTexture> texture, bool initialized,
    std::optional<std::vector<std::shared_ptr<GPUSharedFenceState>>> fences) {
  if (!texture) {
    throw std::runtime_error(
        "GPUSharedTextureMemory::beginAccess(): texture is null");
  }
  // Fails like an access Dawn rejects: a validation error, then the throw.
  if (!_instance) {
    injectDestroyedError("beginAccess");
    throw std::runtime_error(
        "GPUSharedTextureMemory::beginAccess(): the memory was destroyed");
  }

  // Built in lockstep so fenceCount covers both arrays, and kept in locals so
  // the raw pointers outlive the synchronous BeginAccess() below.
  std::vector<wgpu::SharedFence> rawFences;
  std::vector<uint64_t> values;
  if (fences.has_value()) {
    for (const auto &state : *fences) {
      if (state && state->fence) {
        auto fence = state->fence->get();
        if (!fence) {
          state->fence->injectDestroyedError("beginAccess");
          throw std::runtime_error(
              "GPUSharedTextureMemory::beginAccess(): a fence was destroyed");
        }
        rawFences.push_back(std::move(fence));
        values.push_back(state->signaledValue);
      }
    }
  }

  // The access descriptor (and, on Vulkan, the layout the image is acquired
  // in) is the one of every native frame import: see NativeFrameImport.cpp.
  native_frame::beginAccess(_instance, texture->get(), initialized, rawFences,
                            values, "GPUSharedTextureMemory::beginAccess");
  _accessed.push_back(texture->get());
}

jsi::Value GPUSharedTextureMemory::endAccess(jsi::Runtime &runtime,
                                             const jsi::Value &,
                                             const jsi::Value *args,
                                             size_t count) {
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(
        runtime, "GPUSharedTextureMemory::endAccess(): expected (texture)");
  }
  if (!_instance) {
    injectDestroyedError("endAccess");
    throw jsi::JSError(
        runtime,
        "GPUSharedTextureMemory::endAccess(): the memory was destroyed");
  }
  auto texture = GPUTexture::fromValue(runtime, args[0]);

  wgpu::SharedTextureMemoryEndAccessState state{};
  try {
    native_frame::endAccess(_instance, texture->get(), &state,
                            "GPUSharedTextureMemory::endAccess");
  } catch (const std::exception &e) {
    throw jsi::JSError(runtime, e.what());
  }
  auto ended = texture->get();
  _accessed.erase(std::remove_if(_accessed.begin(), _accessed.end(),
                                 [&](const wgpu::Texture &accessed) {
                                   return accessed.Get() == ended.Get();
                                 }),
                  _accessed.end());

  // Copy each wgpu::SharedFence (ref-counted) into its own GPUSharedFence
  // wrapper before `state` is destroyed.
  jsi::Array fences(runtime, state.fenceCount);
  for (size_t i = 0; i < state.fenceCount; i++) {
    wgpu::SharedFence fence = state.fences[i];
    auto wrapper =
        std::make_shared<GPUSharedFence>(_device, std::move(fence), "");
    jsi::Object entry(runtime);
    entry.setProperty(runtime, "fence",
                      GPUSharedFence::create(runtime, std::move(wrapper)));
    entry.setProperty(
        runtime, "signaledValue",
        jsi::BigInt::fromUint64(runtime, state.signaledValues[i]));
    fences.setValueAtIndex(runtime, i, std::move(entry));
  }

  jsi::Object result(runtime);
  result.setProperty(runtime, "initialized",
                     jsi::Value(static_cast<bool>(state.initialized)));
  result.setProperty(runtime, "fences", std::move(fences));
  return result;
}

} // namespace rnwgpu
