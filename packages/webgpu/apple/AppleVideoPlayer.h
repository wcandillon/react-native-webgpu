#pragma once

#include "PlatformContext.h"

#include <memory>
#include <string>

#ifdef __OBJC__
#import <CoreVideo/CoreVideo.h>
#endif

namespace rnwgpu {

// Factory: creates a new IVideoPlayer backed by AVPlayer +
// AVPlayerItemVideoOutput. `format` selects the surface layout.
std::unique_ptr<IVideoPlayer>
createAppleVideoPlayer(const std::string &path, VideoPixelFormat format);

// Generate a small procedurally-animated test video and write it to a
// temporary file. Returns the absolute path. Used by the SharedTextureMemory
// example so it doesn't need a bundled .mp4.
std::string writeAppleTestVideoFile();

#ifdef __OBJC__
// Build a VideoFrameHandle from an existing CVPixelBuffer. CFRetains the
// pixel buffer so the caller can release their reference immediately. Reads
// IOSurface, dimensions, pixel format, and YUV matrix off the buffer.
VideoFrameHandle wrapCVPixelBuffer(CVPixelBufferRef pixelBuffer);
#endif

} // namespace rnwgpu
