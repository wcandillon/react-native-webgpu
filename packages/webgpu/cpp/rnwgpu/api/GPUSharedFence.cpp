#include "GPUSharedFence.h"

#include <cstdint>
#include <string>

namespace rnwgpu {

namespace {

// Kebab-case names matching the shared-fence-* feature strings (see Unions.h /
// GPUFeatures.h).
std::string sharedFenceTypeToString(wgpu::SharedFenceType type) {
  switch (type) {
  case wgpu::SharedFenceType::MTLSharedEvent:
    return "mtl-shared-event";
  case wgpu::SharedFenceType::SyncFD:
    return "sync-fd";
  case wgpu::SharedFenceType::VkSemaphoreOpaqueFD:
    return "vk-semaphore-opaque-fd";
  case wgpu::SharedFenceType::VkSemaphoreZirconHandle:
    return "vk-semaphore-zircon-handle";
  case wgpu::SharedFenceType::DXGISharedHandle:
    return "dxgi-shared-handle";
  case wgpu::SharedFenceType::EGLSync:
    return "egl-sync";
  default:
    return "";
  }
}

} // namespace

jsi::Value GPUSharedFence::exportInfo(jsi::Runtime &runtime, const jsi::Value &,
                                      const jsi::Value *, size_t) {
  wgpu::SharedFenceExportInfo info{};
  uint64_t handle = 0;

#if defined(__APPLE__)
  // Apple: the handle is an id<MTLSharedEvent> pointer.
  wgpu::SharedFenceMTLSharedEventExportInfo mtlInfo{};
  info.nextInChain = &mtlInfo;
  _instance.ExportInfo(&info);
  handle = reinterpret_cast<uint64_t>(mtlInfo.sharedEvent);
#elif defined(__ANDROID__)
  // Android: the handle is an OS file descriptor (sync_fd).
  wgpu::SharedFenceSyncFDExportInfo fdInfo{};
  info.nextInChain = &fdInfo;
  _instance.ExportInfo(&info);
  handle = static_cast<uint64_t>(static_cast<uint32_t>(fdInfo.handle));
#else
  _instance.ExportInfo(&info);
#endif

  jsi::Object result(runtime);
  result.setProperty(
      runtime, "type",
      jsi::String::createFromUtf8(runtime, sharedFenceTypeToString(info.type)));
  result.setProperty(runtime, "handle",
                     jsi::BigInt::fromUint64(runtime, handle));
  return result;
}

} // namespace rnwgpu
