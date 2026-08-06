#pragma once

#include <string>

#include <cstdio>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// Single source of truth for the wgpu::FeatureName <-> GPUFeatureName string
// mapping. Every conversion (both directions, here and in EnumMapper, see
// api/descriptors/Unions.h) is derived from this list so the tables cannot
// drift apart.
//
// The list must cover every value of wgpu::FeatureName. Dawn adds features on
// every milestone upgrade; FeatureNames.spec.ts diffs this list against the
// enum in the installed webgpu_cpp.h and fails when one is missing.
#define RNWGPU_FOR_EACH_FEATURE_NAME(V)                                        \
  V(CoreFeaturesAndLimits, "core-features-and-limits")                         \
  V(DepthClipControl, "depth-clip-control")                                    \
  V(Depth32FloatStencil8, "depth32float-stencil8")                             \
  V(TextureCompressionBC, "texture-compression-bc")                            \
  V(TextureCompressionBCSliced3D, "texture-compression-bc-sliced-3d")          \
  V(TextureCompressionETC2, "texture-compression-etc2")                        \
  V(TextureCompressionASTC, "texture-compression-astc")                        \
  V(TextureCompressionASTCSliced3D, "texture-compression-astc-sliced-3d")      \
  V(TimestampQuery, "timestamp-query")                                         \
  V(IndirectFirstInstance, "indirect-first-instance")                          \
  V(ShaderF16, "shader-f16")                                                   \
  V(RG11B10UfloatRenderable, "rg11b10ufloat-renderable")                       \
  V(BGRA8UnormStorage, "bgra8unorm-storage")                                   \
  V(Float32Filterable, "float32-filterable")                                   \
  V(Float32Blendable, "float32-blendable")                                     \
  V(ClipDistances, "clip-distances")                                           \
  V(DualSourceBlending, "dual-source-blending")                                \
  V(Subgroups, "subgroups")                                                    \
  V(TextureFormatsTier1, "texture-formats-tier1")                              \
  V(TextureFormatsTier2, "texture-formats-tier2")                              \
  V(PrimitiveIndex, "primitive-index")                                         \
  V(TextureComponentSwizzle, "texture-component-swizzle")                      \
  V(SubgroupSizeControl, "subgroup-size-control")                              \
  V(DawnInternalUsages, "dawn-internal-usages")                                \
  V(DawnMultiPlanarFormats, "dawn-multi-planar-formats")                       \
  V(DawnNative, "dawn-native")                                                 \
  V(ChromiumExperimentalTimestampQueryInsidePasses,                            \
    "chromium-experimental-timestamp-query-inside-passes")                     \
  V(ImplicitDeviceSynchronization, "implicit-device-synchronization")          \
  V(TransientAttachments, "transient-attachments")                             \
  V(MSAARenderToSingleSampled, "msaa-render-to-single-sampled")                \
  V(D3D11MultithreadProtected, "d3d11-multithread-protected")                  \
  V(ANGLETextureSharing, "angle-texture-sharing")                              \
  V(PixelLocalStorageCoherent, "pixel-local-storage-coherent")                 \
  V(PixelLocalStorageNonCoherent, "pixel-local-storage-non-coherent")          \
  V(Unorm16TextureFormats, "unorm16-texture-formats")                          \
  V(MultiPlanarFormatExtendedUsages, "multi-planar-format-extended-usages")    \
  V(MultiPlanarFormatP010, "multi-planar-format-p010")                         \
  V(HostMappedPointer, "host-mapped-pointer")                                  \
  V(MultiPlanarRenderTargets, "multi-planar-render-targets")                   \
  V(MultiPlanarFormatNv12a, "multi-planar-format-nv12a")                       \
  V(FramebufferFetch, "framebuffer-fetch")                                     \
  V(BufferMapExtendedUsages, "buffer-map-extended-usages")                     \
  V(AdapterPropertiesMemoryHeaps, "adapter-properties-memory-heaps")           \
  V(AdapterPropertiesD3D, "adapter-properties-d3d")                            \
  V(AdapterPropertiesVk, "adapter-properties-vk")                              \
  V(DawnFormatCapabilities, "format-capabilities")                             \
  V(DawnDrmFormatCapabilities, "drm-format-capabilities")                      \
  V(MultiPlanarFormatNv16, "multi-planar-format-nv16")                         \
  V(MultiPlanarFormatNv24, "multi-planar-format-nv24")                         \
  V(MultiPlanarFormatP210, "multi-planar-format-p210")                         \
  V(MultiPlanarFormatP410, "multi-planar-format-p410")                         \
  V(SharedTextureMemoryVkDedicatedAllocation,                                  \
    "shared-texture-memory-vk-dedicated-allocation")                           \
  V(SharedTextureMemoryAHardwareBuffer,                                        \
    "shared-texture-memory-ahardware-buffer")                                  \
  V(SharedTextureMemoryDmaBuf, "shared-texture-memory-dma-buf")                \
  V(SharedTextureMemoryOpaqueFD, "shared-texture-memory-opaque-fd")            \
  V(SharedTextureMemoryZirconHandle, "shared-texture-memory-zircon-handle")    \
  V(SharedTextureMemoryDXGISharedHandle,                                       \
    "shared-texture-memory-dxgi-shared-handle")                                \
  V(SharedTextureMemoryD3D11Texture2D,                                         \
    "shared-texture-memory-d3d11-texture2d")                                   \
  V(SharedTextureMemoryIOSurface, "shared-texture-memory-iosurface")           \
  V(SharedTextureMemoryEGLImage, "shared-texture-memory-egl-image")            \
  V(SharedFenceVkSemaphoreOpaqueFD, "shared-fence-vk-semaphore-opaque-fd")     \
  V(SharedFenceSyncFD, "shared-fence-sync-fd")                                 \
  V(SharedFenceVkSemaphoreZirconHandle,                                        \
    "shared-fence-vk-semaphore-zircon-handle")                                 \
  V(SharedFenceDXGISharedHandle, "shared-fence-dxgi-shared-handle")            \
  V(SharedFenceMTLSharedEvent, "shared-fence-mtl-shared-event")                \
  V(SharedBufferMemoryD3D12Resource, "shared-buffer-memory-d3d12-resource")    \
  V(StaticSamplers, "static-samplers")                                         \
  V(YCbCrVulkanSamplers, "ycbcr-vulkan-samplers")                              \
  V(ShaderModuleCompilationOptions, "shader-module-compilation-options")       \
  V(DawnLoadResolveTexture, "dawn-load-resolve-texture")                       \
  V(DawnPartialLoadResolveTexture, "dawn-partial-load-resolve-texture")        \
  V(MultiDrawIndirect, "multi-draw-indirect")                                  \
  V(DawnTexelCopyBufferRowAlignment, "dawn-texel-copy-buffer-row-alignment")   \
  V(FlexibleTextureViews, "flexible-texture-views")                            \
  V(ChromiumExperimentalSubgroupMatrix,                                        \
    "chromium-experimental-subgroups-matrix")                                  \
  V(SharedFenceEGLSync, "shared-fence-egl-sync")                               \
  V(DawnDeviceAllocatorControl, "dawn-device-allocator-control")               \
  V(AdapterPropertiesWGPU, "adapter-properties-wgpu")                          \
  V(SharedBufferMemoryFromWindowsHandle,                                       \
    "shared-buffer-memory-from-windows-handle")                                \
  V(SharedTextureMemoryD3D12Resource, "shared-texture-memory-d3d12-resource")  \
  V(ChromiumExperimentalSamplingResourceTable,                                 \
    "chromium-experimental-sampling-resource-table")                           \
  V(AtomicVec2uMinMax, "atomic-vec2u-min-max")                                 \
  V(Unorm16FormatsForExternalTexture, "unorm16-formats-for-external-texture")  \
  V(OpaqueYCbCrAndroidForExternalTexture,                                      \
    "opaque-ycbcr-android-for-external-texture")                               \
  V(Unorm16Filterable, "unorm16-filterable")                                   \
  V(RenderPassRenderArea, "render-pass-render-area")                           \
  V(AdapterPropertiesDrm, "adapter-properties-drm")                            \
  V(TextureCompressionUnaligned, "texture-compression-unaligned")              \
  V(DawnAllowUndefinedLoadStoreOp, "dawn-allow-undefined-load-store-op")

static void convertEnumToJSUnion(wgpu::FeatureName inEnum,
                                 std::string *outUnion) {
  switch (inEnum) {
#define RNWGPU_FEATURE_NAME_CASE(Enum, Name)                                   \
  case wgpu::FeatureName::Enum:                                                \
    *outUnion = Name;                                                          \
    break;
    RNWGPU_FOR_EACH_FEATURE_NAME(RNWGPU_FEATURE_NAME_CASE)
#undef RNWGPU_FEATURE_NAME_CASE
  default:
    fprintf(stderr, "Unknown feature name %d\n", static_cast<int>(inEnum));
    *outUnion = "";
  }
}

} // namespace rnwgpu
