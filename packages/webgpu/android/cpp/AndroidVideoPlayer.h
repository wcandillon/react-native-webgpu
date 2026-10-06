#pragma once

#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <fbjni/fbjni.h>
#include <jni.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "PlatformContext.h"

namespace rnwgpu {

// The message of the pending Java exception, which is cleared.
inline std::string describeAndClearJavaException(JNIEnv *env) {
  jthrowable throwable = env->ExceptionOccurred();
  if (throwable == nullptr) {
    return "unknown error";
  }
  env->ExceptionClear();
  std::string message = "Java exception";
  jclass throwableClass = env->GetObjectClass(throwable);
  jmethodID getMessage =
      env->GetMethodID(throwableClass, "getMessage", "()Ljava/lang/String;");
  if (getMessage != nullptr) {
    auto text =
        static_cast<jstring>(env->CallObjectMethod(throwable, getMessage));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    } else if (text != nullptr) {
      const char *chars = env->GetStringUTFChars(text, nullptr);
      if (chars != nullptr) {
        message = chars;
        env->ReleaseStringUTFChars(text, chars);
      }
      env->DeleteLocalRef(text);
    }
  }
  env->DeleteLocalRef(throwableClass);
  env->DeleteLocalRef(throwable);
  return message;
}

// A global reference to the Java player, shared between AndroidVideoPlayer
// and the deleters of the frames it handed out (they close each frame's
// Image through the player). The reference goes away with the last of them.
class JavaVideoPlayerRef {
public:
  JavaVideoPlayerRef(JNIEnv *env, jobject local)
      : _object(env->NewGlobalRef(local)) {}
  ~JavaVideoPlayerRef() {
    facebook::jni::Environment::ensureCurrentThreadIsAttached();
    JNIEnv *env = facebook::jni::Environment::current();
    if (env != nullptr && _object != nullptr) {
      env->DeleteGlobalRef(_object);
    }
  }
  JavaVideoPlayerRef(const JavaVideoPlayerRef &) = delete;
  JavaVideoPlayerRef &operator=(const JavaVideoPlayerRef &) = delete;

  jobject get() const { return _object; }

private:
  jobject _object;
};

// IVideoPlayer backed by com.webgpu.WebGPUVideoPlayer: MediaPlayer decodes
// into an ImageReader whose buffers the GPU samples. The frames keep the
// decoder's native YUV layout (reported as NV12), for importExternalTexture.
class AndroidVideoPlayer : public IVideoPlayer {
public:
  AndroidVideoPlayer(JNIEnv *env, jobject player)
      : _player(std::make_shared<JavaVideoPlayerRef>(env, player)) {
    jclass playerClass = env->GetObjectClass(player);
    _copyLatestFrame =
        method(env, playerClass, "copyLatestFrame", "()Landroid/media/Image;");
    _closeImage =
        method(env, playerClass, "closeImage", "(Landroid/media/Image;)V");
    _play = method(env, playerClass, "play", "()V");
    _pause = method(env, playerClass, "pause", "()V");
    _getPaused = method(env, playerClass, "getPaused", "()Z");
    _getCurrentTime = method(env, playerClass, "getCurrentTime", "()D");
    _seek = method(env, playerClass, "seek", "(D)V");
    _getDuration = method(env, playerClass, "getDuration", "()D");
    _getVolume = method(env, playerClass, "getVolume", "()D");
    _setVolume = method(env, playerClass, "setVolume", "(D)V");
    _getLoop = method(env, playerClass, "getLoop", "()Z");
    _setLoop = method(env, playerClass, "setLoop", "(Z)V");
    _getVideoWidth = method(env, playerClass, "getVideoWidth", "()I");
    _getVideoHeight = method(env, playerClass, "getVideoHeight", "()I");
    _getRotation = method(env, playerClass, "getRotation", "()I");
    _getFrameRate = method(env, playerClass, "getFrameRate", "()D");
    _release = method(env, playerClass, "release", "()V");
    env->DeleteLocalRef(playerClass);

    jclass imageClass = env->FindClass("android/media/Image");
    _getHardwareBuffer = method(env, imageClass, "getHardwareBuffer",
                                "()Landroid/hardware/HardwareBuffer;");
    env->DeleteLocalRef(imageClass);
  }

  ~AndroidVideoPlayer() override {
    JNIEnv *env = attach();
    env->CallVoidMethod(_player->get(), _release);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
  }

  VideoFrameHandle copyLatestFrame() override {
    JNIEnv *env = attach();
    jobject image = env->CallObjectMethod(_player->get(), _copyLatestFrame);
    if (env->ExceptionCheck()) {
      throw std::runtime_error("copyLatestFrame: " +
                               describeAndClearJavaException(env));
    }
    if (image == nullptr) {
      return {};
    }

    jobject hardwareBuffer = env->CallObjectMethod(image, _getHardwareBuffer);
    if (env->ExceptionCheck() || hardwareBuffer == nullptr) {
      env->ExceptionClear();
      closeImage(env, image);
      env->DeleteLocalRef(image);
      throw std::runtime_error(
          "copyLatestFrame: the decoded frame has no HardwareBuffer");
    }
    AHardwareBuffer *buffer =
        AHardwareBuffer_fromHardwareBuffer(env, hardwareBuffer);
    env->DeleteLocalRef(hardwareBuffer);

    AHardwareBuffer_Desc desc = {};
    AHardwareBuffer_describe(buffer, &desc);
    if ((desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) == 0) {
      closeImage(env, image);
      env->DeleteLocalRef(image);
      throw std::runtime_error(
          "copyLatestFrame: the decoder allocated frames without "
          "AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, so the GPU cannot sample "
          "them");
    }

    // The frame owns a reference to the buffer and keeps the Image acquired
    // until it is released; closing the Image hands the buffer back to the
    // decoder.
    AHardwareBuffer_acquire(buffer);
    jobject imageRef = env->NewGlobalRef(image);
    env->DeleteLocalRef(image);

    VideoFrameHandle handle;
    handle.handle = static_cast<void *>(buffer);
    handle.width = desc.width;
    handle.height = desc.height;
    handle.pixelFormat = isRgbFormat(desc.format) ? VideoPixelFormat::BGRA8
                                                  : VideoPixelFormat::NV12;
    auto player = _player;
    jmethodID closeImageMethod = _closeImage;
    handle.deleter = [player, closeImageMethod, imageRef, buffer]() {
      JNIEnv *env = attach();
      env->CallVoidMethod(player->get(), closeImageMethod, imageRef);
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      }
      env->DeleteGlobalRef(imageRef);
      AHardwareBuffer_release(buffer);
    };
    return handle;
  }

  void play() override { callVoid(_play); }
  void pause() override { callVoid(_pause); }
  bool paused() override { return callBool(_getPaused); }

  double currentTime() override { return callDouble(_getCurrentTime); }
  void seek(double seconds) override {
    JNIEnv *env = attach();
    env->CallVoidMethod(_player->get(), _seek, static_cast<jdouble>(seconds));
    clearException(env);
  }
  double duration() override { return callDouble(_getDuration); }

  double volume() override { return callDouble(_getVolume); }
  void setVolume(double volume) override {
    JNIEnv *env = attach();
    env->CallVoidMethod(_player->get(), _setVolume,
                        static_cast<jdouble>(volume));
    clearException(env);
  }

  bool loop() override { return callBool(_getLoop); }
  void setLoop(bool loop) override {
    JNIEnv *env = attach();
    env->CallVoidMethod(_player->get(), _setLoop, static_cast<jboolean>(loop));
    clearException(env);
  }

  uint32_t videoWidth() override {
    return static_cast<uint32_t>(callInt(_getVideoWidth));
  }
  uint32_t videoHeight() override {
    return static_cast<uint32_t>(callInt(_getVideoHeight));
  }
  int rotation() override { return callInt(_getRotation); }
  double frameRate() override { return callDouble(_getFrameRate); }

private:
  static JNIEnv *attach() {
    facebook::jni::Environment::ensureCurrentThreadIsAttached();
    JNIEnv *env = facebook::jni::Environment::current();
    if (env == nullptr) {
      throw std::runtime_error("VideoPlayer: no JNI environment");
    }
    return env;
  }

  static jmethodID method(JNIEnv *env, jclass cls, const char *name,
                          const char *signature) {
    jmethodID id = env->GetMethodID(cls, name, signature);
    if (id == nullptr) {
      env->ExceptionClear();
      throw std::runtime_error(std::string("VideoPlayer: method ") + name +
                               " not found");
    }
    return id;
  }

  static void clearException(JNIEnv *env) {
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
  }

  // Single-plane RGB buffers sample as regular textures; everything else
  // (YUV 4:2:0, P010, implementation-defined codec formats) goes through
  // Vulkan's YCbCr conversion as an external texture.
  static bool isRgbFormat(uint32_t format) {
    switch (format) {
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
    case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
    case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
      return true;
    default:
      return false;
    }
  }

  void closeImage(JNIEnv *env, jobject image) {
    env->CallVoidMethod(_player->get(), _closeImage, image);
    clearException(env);
  }

  void callVoid(jmethodID id) {
    JNIEnv *env = attach();
    env->CallVoidMethod(_player->get(), id);
    clearException(env);
  }

  bool callBool(jmethodID id) {
    JNIEnv *env = attach();
    bool result = env->CallBooleanMethod(_player->get(), id) == JNI_TRUE;
    clearException(env);
    return result;
  }

  int callInt(jmethodID id) {
    JNIEnv *env = attach();
    int result = env->CallIntMethod(_player->get(), id);
    clearException(env);
    return result;
  }

  double callDouble(jmethodID id) {
    JNIEnv *env = attach();
    double result = env->CallDoubleMethod(_player->get(), id);
    clearException(env);
    return result;
  }

  std::shared_ptr<JavaVideoPlayerRef> _player;
  jmethodID _copyLatestFrame;
  jmethodID _closeImage;
  jmethodID _play;
  jmethodID _pause;
  jmethodID _getPaused;
  jmethodID _getCurrentTime;
  jmethodID _seek;
  jmethodID _getDuration;
  jmethodID _getVolume;
  jmethodID _setVolume;
  jmethodID _getLoop;
  jmethodID _setLoop;
  jmethodID _getVideoWidth;
  jmethodID _getVideoHeight;
  jmethodID _getRotation;
  jmethodID _getFrameRate;
  jmethodID _release;
  jmethodID _getHardwareBuffer;
};

} // namespace rnwgpu
