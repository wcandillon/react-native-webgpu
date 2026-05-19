#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "NativeObject.h"
#include "PlatformContext.h"
#include "VideoFrame.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// JSI wrapper around a platform-specific IVideoPlayer. Hands out fresh
// VideoFrame handles each time the underlying decoder produces a new frame.
class VideoPlayer : public NativeObject<VideoPlayer> {
public:
  static constexpr const char *CLASS_NAME = "VideoPlayer";

  explicit VideoPlayer(std::unique_ptr<IVideoPlayer> impl)
      : NativeObject(CLASS_NAME), _impl(std::move(impl)) {}

  std::string getBrand() { return CLASS_NAME; }

  // Returns the latest decoded frame, or null if no new frame is ready yet.
  // Callers should poll this from their render loop and skip rendering (or
  // reuse the last frame's texture) when null.
  std::variant<std::nullptr_t, std::shared_ptr<VideoFrame>>
  copyLatestFrame() {
    if (!_impl) {
      return nullptr;
    }
    auto handle = _impl->copyLatestFrame();
    if (handle.handle == nullptr) {
      return nullptr;
    }
    return std::make_shared<VideoFrame>(handle.handle, handle.width,
                                        handle.height,
                                        std::move(handle.deleter));
  }

  void play() {
    if (_impl) {
      _impl->play();
    }
  }

  void pause() {
    if (_impl) {
      _impl->pause();
    }
  }

  void release() { _impl.reset(); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &VideoPlayer::getBrand);
    installMethod(runtime, prototype, "copyLatestFrame",
                  &VideoPlayer::copyLatestFrame);
    installMethod(runtime, prototype, "play", &VideoPlayer::play);
    installMethod(runtime, prototype, "pause", &VideoPlayer::pause);
    installMethod(runtime, prototype, "release", &VideoPlayer::release);
  }

private:
  std::unique_ptr<IVideoPlayer> _impl;
};

} // namespace rnwgpu
