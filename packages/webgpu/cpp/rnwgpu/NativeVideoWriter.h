#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"

namespace rnwgpu {

// Why a video writer failed, as the `code` of the error it throws or its
// promises reject with:
//
//   - "unsupported": the platform cannot encode these options (a size or
//     frame rate out of range, no H.264 encoder).
//   - "invalid-argument": an option or an argument is wrong (a buffer that
//     did not come from this writer, a timestamp that goes back).
//   - "write-failed": the file could not be written (a bad path, a full
//     disk, the writer failed).
//   - "audio-failed": the sound file could not be read or muxed.
//   - "encoder-failed": the encoder failed while encoding, or a frame's GPU
//     work never finished.
//   - "interrupted": the system took the encoder or the GPU away: the iOS
//     app went to the background, Android reclaimed the codec.
//   - "cancelled": the writer was cancelled.
class VideoWriterError : public std::runtime_error {
public:
  VideoWriterError(std::string code, const std::string &message)
      : std::runtime_error(message), _code(std::move(code)) {}

  const std::string &code() const { return _code; }

private:
  std::string _code;
};

struct VideoWriterOptions {
  // The output .mp4, an absolute file path.
  std::string path;
  uint32_t width = 0;
  uint32_t height = 0;
  // The nominal frame rate, for the encoder's rate control and key frame
  // interval; each frame shows at the timestamp it is appended with.
  double frameRate = 30;
  int64_t bitrate = 12'000'000;
  // A sound file muxed from t = 0, cut where the video ends.
  std::optional<std::string> audioPath;
  // The device the frames are drawn with. Android presents each frame onto
  // the encoder's input surface with it; Apple platforms hand the buffer
  // itself to the encoder and do not need it.
  wgpu::Device device;
};

using VideoWriterErrorCallback =
    std::function<void(const std::string &code, const std::string &message)>;

// Encodes frames drawn by the GPU into an H.264 .mp4 file with the platform's
// hardware encoder, the frames staying on the GPU: the writer hands out the
// buffers it encodes (dequeueBuffer), the caller draws each one through
// importSharedTextureMemory and appends it with the fences of its access.
//
// dequeueBuffer, append, finish and cancel are called from one JS thread.
// The callbacks run on any thread.
class IVideoWriter {
public:
  virtual ~IVideoWriter() = default;

  // Calls `onBuffer` with a buffer to draw the next frame into once the
  // encoder can take one, or `onError` once the writer failed. The buffer is
  // the caller's until it is appended (or released, which gives it back
  // unencoded).
  virtual void dequeueBuffer(std::function<void(VideoFrameHandle)> onBuffer,
                             VideoWriterErrorCallback onError) = 0;

  // Encodes `buffer` (a handle dequeueBuffer handed out) as the frame shown
  // at `timestamp` seconds, once `fence` reaches `signaledValue` (no fence:
  // the buffer is complete already). Returns at once. Throws
  // VideoWriterError for a call that can never succeed; a failure while
  // encoding is reported by the next dequeueBuffer and by finish.
  virtual void append(void *buffer, double timestamp, wgpu::SharedFence fence,
                      uint64_t signaledValue) = 0;

  // Writes the frames appended so far, then the sound, and closes the file.
  virtual void finish(std::function<void()> onDone,
                      VideoWriterErrorCallback onError) = 0;

  // Stops the encoder and deletes the partial file. Pending callbacks get a
  // "cancelled" error. `onDone` (if set, from any thread) once the file is
  // deleted. Never throws.
  virtual void cancel(std::function<void()> onDone) noexcept = 0;
};

#if defined(__ANDROID__)
// android/cpp/AndroidVideoWriter.cpp
std::unique_ptr<IVideoWriter>
createAndroidVideoWriter(const VideoWriterOptions &options);
#elif defined(__APPLE__)
// apple/AppleVideoWriter.mm
std::unique_ptr<IVideoWriter>
createAppleVideoWriter(const VideoWriterOptions &options);
#endif

} // namespace rnwgpu
