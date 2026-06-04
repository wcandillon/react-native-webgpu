#include "Embedding.h"

// Internal bindings headers. These are compiled as part of react-native-wgpu's
// own pod, so their bare/prefixed includes resolve against this package's source
// tree. This translation unit is the single bridge between the host-facing
// Embedding API and the internal rnwgpu::* implementation.
#include "rnwgpu/api/GPUDevice.h"
#include "rnwgpu/api/GPUTexture.h"
#include "rnwgpu/async/AsyncRunner.h"

namespace react_native_wgpu {

jsi::Value makeDeviceJSI(jsi::Runtime &runtime, wgpu::Device device,
                         const std::string &label) {
  auto async = rnwgpu::async::AsyncRunner::get(runtime);
  if (!async) {
    throw jsi::JSError(
        runtime,
        "[react-native-wgpu] AsyncRunner not initialized for this runtime; "
        "ensure react-native-wgpu is installed before calling makeDeviceJSI()");
  }
  auto wrapped = std::make_shared<rnwgpu::GPUDevice>(device, async, label);
  return rnwgpu::GPUDevice::create(runtime, wrapped);
}

jsi::Value makeTextureJSI(jsi::Runtime &runtime, wgpu::Texture texture,
                          const std::string &label) {
  auto wrapped = std::make_shared<rnwgpu::GPUTexture>(texture, label);
  return rnwgpu::GPUTexture::create(runtime, wrapped);
}

wgpu::Texture textureFromJSI(jsi::Runtime &runtime, const jsi::Value &value) {
  if (!value.isObject()) {
    return nullptr;
  }
  auto object = value.getObject(runtime);
  if (!object.hasNativeState(runtime)) {
    return nullptr;
  }
  auto texture = object.getNativeState<rnwgpu::GPUTexture>(runtime);
  if (!texture) {
    return nullptr;
  }
  return texture->get();
}

} // namespace react_native_wgpu
