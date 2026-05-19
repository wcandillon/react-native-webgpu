#pragma once

#include "PlatformContext.h"

#include <memory>
#include <string>

namespace rnwgpu {

// Factory: creates a new IVideoPlayer backed by AVPlayer +
// AVPlayerItemVideoOutput. `format` selects the surface layout.
std::unique_ptr<IVideoPlayer>
createAppleVideoPlayer(const std::string &path, VideoPixelFormat format);

// Generate a small procedurally-animated test video and write it to a
// temporary file. Returns the absolute path. Used by the SharedTextureMemory
// example so it doesn't need a bundled .mp4.
std::string writeAppleTestVideoFile();

} // namespace rnwgpu
