#include "GPUSharedTextureMemory.h"

#include <memory>
#include <stdexcept>
#include <string>

#include "Convertors.h"

namespace rnwgpu {

void GPUSharedTextureMemory::destroy() {
  if (_instance) {
    (void)_instance.GetProperties(&_properties);
    _instance = nullptr;
  }
}

void GPUSharedTextureMemory::injectDestroyedError(const char *method) {
  auto object = _label.empty() ? std::string("GPUSharedTextureMemory")
                               : "GPUSharedTextureMemory \"" + _label + "\"";
  auto message = "Destroyed " + object + " used in " + method + "().";
  _device.InjectError(wgpu::ErrorType::Validation, message.c_str());
}

std::shared_ptr<GPUTexture> GPUSharedTextureMemory::createTexture(
    std::optional<std::shared_ptr<GPUTextureDescriptor>> descriptor) {
  if (!_instance) {
    // Like a texture Dawn can't create from the memory: the validation error
    // says why, and the texture is an error texture with the memory's
    // properties (an undefined format would make its getters throw).
    injectDestroyedError("createTexture");
    wgpu::TextureDescriptor desc{};
    desc.usage = _properties.usage;
    desc.size = _properties.size;
    desc.format = _properties.format;
    return std::make_shared<GPUTexture>(_device.CreateErrorTexture(&desc), "");
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
  wgpu::SharedTextureMemoryBeginAccessDescriptor desc{};
  desc.initialized = initialized;
  desc.concurrentRead = false;

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
  if (!rawFences.empty()) {
    desc.fenceCount = rawFences.size();
    desc.fences = rawFences.data();
    desc.signaledValueCount = values.size();
    desc.signaledValues = values.data();
  } else {
    desc.fenceCount = 0;
    desc.fences = nullptr;
    desc.signaledValueCount = 0;
    desc.signaledValues = nullptr;
  }

#if defined(__ANDROID__)
  // Dawn's Vulkan backend (AHardwareBuffer) validates that the begin-access
  // descriptor chains a SharedTextureMemoryVkImageLayoutBeginState specifying
  // the VkImageLayout to acquire the image into. UNDEFINED (= 0) on both ends
  // is the canonical "no prior GPU producer" pattern: Dawn performs an
  // external-queue acquire from VK_QUEUE_FAMILY_EXTERNAL which preserves the
  // AHB contents, then transitions to whatever layout the texture's actual
  // usage requires.
  wgpu::SharedTextureMemoryVkImageLayoutBeginState vkLayout{};
  vkLayout.oldLayout = 0;
  vkLayout.newLayout = 0;
  desc.nextInChain = &vkLayout;
#endif

  auto status = _instance.BeginAccess(texture->get(), &desc);
  if (status != wgpu::Status::Success) {
    throw std::runtime_error("GPUSharedTextureMemory::beginAccess() failed");
  }
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

#if defined(__ANDROID__)
  // Dawn's Vulkan backend writes the released old/new VkImageLayouts back into
  // a chained SharedTextureMemoryVkImageLayoutEndState; validation requires
  // the chain even when the caller doesn't read the values.
  wgpu::SharedTextureMemoryVkImageLayoutEndState vkLayout{};
  state.nextInChain = &vkLayout;
#endif

  auto status = _instance.EndAccess(texture->get(), &state);
  if (status != wgpu::Status::Success) {
    throw jsi::JSError(runtime, "GPUSharedTextureMemory::endAccess() failed");
  }

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
    entry.setProperty(runtime, "signaledValue",
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
