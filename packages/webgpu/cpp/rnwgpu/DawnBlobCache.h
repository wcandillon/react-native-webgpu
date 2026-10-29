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
// and on Vulkan drivers that can take a noticeable time per pipeline.
//
// Each platform's module points the cache at the app's cache directory when it
// installs, and requestDevice() chains it onto every device it creates. A
// device requested with the `disable_blob_cache` Dawn toggle does not use it.
//
// The cache is best effort. By default Dawn checks a hash of every blob it
// loads, and an entry that is torn, truncated or for another key reads as a
// miss. Entries are
// replaced whole, so the writer that finishes last wins: devices or processes
// that share a key (Dawn's monolithic Vulkan pipeline cache has one per
// device) can replace a fuller blob with a smaller one, which only costs the
// pipelines in it being built again.
class DawnBlobCache {
public:
  // The directory to keep the blobs in, created (with its parents) if
  // missing. Each platform's module calls this when it installs, which is
  // also when the cache is tidied: abandoned writes are removed, and the
  // oldest entries with them while the cache is over its limit (about 64 MB,
  // checked again as a session stores). "Oldest" is by when an entry was
  // written, not read. An entry larger than half the limit is not stored. Only
  // the cache's own files are removed. An empty path, or one where no directory
  // can be made, leaves the cache off.
  static void setDirectory(const std::string &directory);

  // Chains the cache onto `descriptor`, in front of whatever it already
  // chains, and returns true. `storage` holds the chained struct and must
  // outlive the RequestDevice call that reads it. Does nothing, and returns
  // false, without a directory.
  static bool chain(wgpu::DeviceDescriptor &descriptor,
                    WGPUDawnCacheDeviceDescriptor &storage);
};

} // namespace rnwgpu
