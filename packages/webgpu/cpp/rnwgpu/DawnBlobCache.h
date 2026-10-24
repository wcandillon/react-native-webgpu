#pragma once

#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// Dawn's blob cache, kept on disk so the pipelines a device compiles (and the
// shaders Dawn translates for them) survive a relaunch. Dawn only caches when
// the embedder gives it somewhere to put the blobs (Chrome hands it its disk
// cache); without one, every launch compiles every pipeline again, which on
// Android's Vulkan drivers costs tens to hundreds of milliseconds each.
//
// Each platform's module points the cache at the app's cache directory when it
// installs, and requestDevice() chains it onto every device it creates. A
// device requested with the `disable_blob_cache` Dawn toggle does not use it.
class DawnBlobCache {
public:
  // The directory to keep the blobs in, created if missing. Leftovers of
  // interrupted writes are removed, and an outgrown cache is cleared. An empty
  // path disables the cache.
  static void setDirectory(const std::string &directory);

  // Chains the cache onto `descriptor`, in front of whatever it already
  // chains. `storage` holds the chained struct and must outlive the
  // RequestDevice call that reads it. Does nothing without a directory.
  static void chain(wgpu::DeviceDescriptor &descriptor,
                    WGPUDawnCacheDeviceDescriptor &storage);
};

} // namespace rnwgpu
