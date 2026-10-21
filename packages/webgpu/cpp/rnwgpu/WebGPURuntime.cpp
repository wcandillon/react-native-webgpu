#include "WebGPURuntime.h"

#include <dlfcn.h>

#include <iterator>

namespace rnwgpu {

namespace {

wgpu::Instance createInstance() {
  // If a Graphite build of react-native-skia is loaded in this
  // process, adopt its wgpu::Instance instead of creating our own, so
  // Graphite's device and every JS-created device live on one instance. That
  // makes pointer-based handoff (RNWebGPU.importDevice of Skia's device)
  // sound: the async pump serves the shared instance, so callbacks on
  // imported devices settle. Discovered at runtime via dlsym so there is no
  // build-time coupling and native-module init order does not matter. Both
  // packages must link the same single Dawn copy (they vendor the same
  // artifact) for the handle to be meaningful.
  using GetInstanceFn = WGPUInstance (*)();
  void *getter = dlsym(RTLD_DEFAULT, "rnskia_getWGPUInstance");
#ifdef __ANDROID__
  // System.loadLibrary loads librnskia.so with RTLD_LOCAL, so RTLD_DEFAULT
  // does not search it. RTLD_NOLOAD returns its handle only if it is already
  // loaded, so this never loads Skia.
  if (getter == nullptr) {
    if (void *skia = dlopen("librnskia.so", RTLD_NOW | RTLD_NOLOAD)) {
      getter = dlsym(skia, "rnskia_getWGPUInstance");
      dlclose(skia);
    }
  }
#endif
  if (auto getSkiaInstance = reinterpret_cast<GetInstanceFn>(getter)) {
    if (WGPUInstance external = getSkiaInstance()) {
      wgpuInstanceAddRef(external);
      return wgpu::Instance::Acquire(external);
    }
  }

  static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instanceDesc{.requiredFeatureCount = 1,
                                        .requiredFeatures = &kTimedWaitAny};

  wgpu::InstanceLimits limits{.timedWaitAnyMaxCount = 64};
  instanceDesc.requiredLimits = &limits;

  // Expose Dawn's experimental adapter features. Several features needed by
  // our Android external-texture path (YCbCrVulkanSamplers,
  // OpaqueYCbCrAndroidForExternalTexture) are tagged Experimental in Dawn's
  // feature table and are otherwise filtered out of adapter.features by
  // PhysicalDeviceBase::GetSupportedFeatures. The allow_unsafe_apis toggle
  // disables that filter so the features become visible;
  // expose_wgsl_experimental_features is the parallel toggle for WGSL language
  // features.
  //
  // Trade-off: these are instance-level Dawn toggles, so they apply to every
  // adapter/device created from this instance, not just the external-texture
  // path. There is no finer-grained, per-feature mechanism to surface these.
  // The exposure is acceptable because the toggle only *un-hides* experimental
  // features in adapter.features; it does not enable any of them. Nothing
  // experimental becomes active unless application code explicitly lists that
  // feature in requiredFeatures at device creation, so the default behavior of
  // a device that asks for no experimental features is unchanged.
  static const char *const kEnabledToggles[] = {
      "allow_unsafe_apis",
      "expose_wgsl_experimental_features",
  };
  wgpu::DawnTogglesDescriptor toggles;
  toggles.enabledToggleCount = std::size(kEnabledToggles);
  toggles.enabledToggles = kEnabledToggles;
  instanceDesc.nextInChain = &toggles;

  return wgpu::CreateInstance(&instanceDesc);
}

} // namespace

WebGPURuntime &WebGPURuntime::shared() {
  static auto *runtime = new WebGPURuntime();
  return *runtime;
}

wgpu::Instance WebGPURuntime::instance() {
  std::call_once(_instanceOnce, [this] { _instance = createInstance(); });
  return _instance;
}

wgpu::Surface WebGPURuntime::makeSurface(void *nativeSurface) {
#if defined(__ANDROID__)
  wgpu::SurfaceSourceAndroidNativeWindow source;
  source.window = nativeSurface;
#else
  wgpu::SurfaceSourceMetalLayer source;
  source.layer = nativeSurface;
#endif
  wgpu::SurfaceDescriptor descriptor;
  descriptor.nextInChain = &source;
  return instance().CreateSurface(&descriptor);
}

} // namespace rnwgpu
