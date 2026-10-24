#pragma once

#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// Dawn's blob cache, kept on disk so what a device produces while building
// shaders and pipelines (Vulkan pipeline data, translated MSL) survives a
// relaunch. Dawn only reuses those blobs across launches when the embedder
// gives it storage: load and store callbacks on the device descriptor, or a
// caching interface on the platform the instance was created with (how Chrome
// does it). The instance can be React Native Skia's, so the callbacks go on
// each device. Without them every launch builds every pipeline from scratch,
// which on Android's Vulkan drivers costs tens to hundreds of milliseconds
// each.
//
// Each platform's module points the cache at the app's cache directory when it
// installs, and requestDevice() chains it onto every device it creates. A
// device requested with the `disable_blob_cache` Dawn toggle does not use it.
class DawnBlobCache {
public:
  // The directory to keep the blobs in, created (with its parents) if
  // missing. Each platform's module calls this when it installs, which is
  // also when the cache is tidied: abandoned writes are removed, and a cache
  // that has outgrown its limit is cleared. Only the cache's own files are
  // removed. An empty path, or one where no directory can be made, leaves the
  // cache off.
  static void setDirectory(const std::string &directory);

  // Chains the cache onto `descriptor`, in front of whatever it already
  // chains. `storage` holds the chained struct and must outlive the
  // RequestDevice call that reads it. Does nothing without a directory.
  static void chain(wgpu::DeviceDescriptor &descriptor,
                    WGPUDawnCacheDeviceDescriptor &storage);
};

} // namespace rnwgpu
