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
// VideoFrame handles each time the underlying decoder produces a new frame,
// and exposes playback controls modeled on HTMLMediaElement (see
// VideoPlayer in src/types.ts).
class VideoPlayer : public NativeObject<VideoPlayer> {
public:
  static constexpr const char *CLASS_NAME = "VideoPlayer";

  explicit VideoPlayer(std::unique_ptr<IVideoPlayer> impl)
      : NativeObject(CLASS_NAME), _impl(std::move(impl)) {}

  std::string getBrand() { return CLASS_NAME; }

  // Returns the latest decoded frame, or null if no new frame is ready yet.
  // Callers should poll this from their render loop and skip rendering (or
  // reuse the last frame's texture) when null.
  std::variant<std::nullptr_t, std::shared_ptr<VideoFrame>> copyLatestFrame() {
    if (!_impl) {
      return nullptr;
    }
    auto handle = _impl->copyLatestFrame();
    if (handle.handle == nullptr) {
      return nullptr;
    }
    return std::make_shared<VideoFrame>(std::move(handle));
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

  // A released player reads as paused, at the start, with no metadata.
  bool getPaused() { return _impl ? _impl->paused() : true; }

  double getCurrentTime() { return _impl ? _impl->currentTime() : 0; }
  void setCurrentTime(double seconds) {
    if (_impl) {
      _impl->seek(seconds);
    }
  }

  double getDuration() { return _impl ? _impl->duration() : 0; }

  double getVolume() { return _impl ? _impl->volume() : 0; }
  void setVolume(double volume) {
    if (_impl) {
      _impl->setVolume(volume);
    }
  }

  bool getLoop() { return _impl ? _impl->loop() : false; }
  void setLoop(bool loop) {
    if (_impl) {
      _impl->setLoop(loop);
    }
  }

  double getVideoWidth() { return _impl ? _impl->videoWidth() : 0; }
  double getVideoHeight() { return _impl ? _impl->videoHeight() : 0; }
  double getRotation() { return _impl ? _impl->rotation() : 0; }
  double getFrameRate() { return _impl ? _impl->frameRate() : 0; }

  void release() { _impl.reset(); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &VideoPlayer::getBrand);
    installMethod(runtime, prototype, "copyLatestFrame",
                  &VideoPlayer::copyLatestFrame);
    installMethod(runtime, prototype, "play", &VideoPlayer::play);
    installMethod(runtime, prototype, "pause", &VideoPlayer::pause);
    installMethod(runtime, prototype, "release", &VideoPlayer::release);
    installGetter(runtime, prototype, "paused", &VideoPlayer::getPaused);
    installGetterSetter(runtime, prototype, "currentTime",
                        &VideoPlayer::getCurrentTime,
                        &VideoPlayer::setCurrentTime);
    installGetter(runtime, prototype, "duration", &VideoPlayer::getDuration);
    installGetterSetter(runtime, prototype, "volume", &VideoPlayer::getVolume,
                        &VideoPlayer::setVolume);
    installGetterSetter(runtime, prototype, "loop", &VideoPlayer::getLoop,
                        &VideoPlayer::setLoop);
    installGetter(runtime, prototype, "videoWidth",
                  &VideoPlayer::getVideoWidth);
    installGetter(runtime, prototype, "videoHeight",
                  &VideoPlayer::getVideoHeight);
    installGetter(runtime, prototype, "rotation", &VideoPlayer::getRotation);
    installGetter(runtime, prototype, "frameRate", &VideoPlayer::getFrameRate);
  }

private:
  std::unique_ptr<IVideoPlayer> _impl;
};

} // namespace rnwgpu
