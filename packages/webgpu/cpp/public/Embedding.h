#pragma once

// Public embedding API for react-native-wgpu.
//
// This is the ONLY header a host package (e.g. React Native Skia with the
// Graphite backend) should include to drive the WebGPU bindings. It depends
// only on the standard library, JSI, and Dawn's C++ header, so including it
// does NOT pull in react-native-wgpu's internal headers (which use bare
// includes and would otherwise collide with the host's own jsi/* utilities).
//
// Installed by CocoaPods under the pod's public header dir, so a host includes:
//   #include <react-native-wgpu/Embedding.h>
//
// Preconditions: react-native-wgpu must already be installed on `runtime`
// (its TurboModule install runs the usual setup: constructors registered and an
// AsyncRunner created for the runtime). These helpers wrap host-owned Dawn
// objects so they cross the JS boundary as the SAME GPUDevice/GPUTexture types
// the bindings hand out, enabling host<->WebGPU interop without a second copy
// of the bindings.

#include <string>

#include <jsi/jsi.h>
#include <webgpu/webgpu_cpp.h>

namespace react_native_wgpu {

namespace jsi = facebook::jsi;

// Wrap a host-owned wgpu::Device as the JS GPUDevice object the bindings expose.
// Throws a jsi::JSError if react-native-wgpu has not been installed on `runtime`
// (no AsyncRunner). Mirrors `new GPUDevice(device, asyncRunner, label)`.
jsi::Value makeDeviceJSI(jsi::Runtime &runtime, wgpu::Device device,
                         const std::string &label);

// Wrap a host-owned wgpu::Texture as a JS GPUTexture object.
jsi::Value makeTextureJSI(jsi::Runtime &runtime, wgpu::Texture texture,
                          const std::string &label);

// Extract the underlying wgpu::Texture from a JS GPUTexture value (one produced
// by makeTextureJSI or by the WebGPU API). Returns a null texture when `value`
// is not a GPUTexture.
wgpu::Texture textureFromJSI(jsi::Runtime &runtime, const jsi::Value &value);

} // namespace react_native_wgpu
