#pragma once

#include <android/bitmap.h>
#include <android/hardware_buffer.h>
#include <jni.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;
namespace jni = facebook::jni;

class AndroidPlatformContext : public PlatformContext {
private:
  jobject _blobModule;

  std::vector<uint8_t> resolveBlob(JNIEnv *env, const std::string &blobId,
                                   double offset, double size) {
    if (!_blobModule) {
      throw std::runtime_error("BlobModule instance is null");
    }

    jclass blobModuleClass = env->GetObjectClass(_blobModule);
    if (!blobModuleClass) {
      throw std::runtime_error("Couldn't find BlobModule class");
    }

    jmethodID resolveMethod = env->GetMethodID(blobModuleClass, "resolve",
                                               "(Ljava/lang/String;II)[B");
    env->DeleteLocalRef(blobModuleClass);

    if (!resolveMethod) {
      throw std::runtime_error("Couldn't find resolve method in BlobModule");
    }

    jstring jBlobId = env->NewStringUTF(blobId.c_str());
    jbyteArray blobData = (jbyteArray)env->CallObjectMethod(
        _blobModule, resolveMethod, jBlobId, static_cast<jint>(offset),
        static_cast<jint>(size));
    env->DeleteLocalRef(jBlobId);

    if (!blobData) {
      throw std::runtime_error("Couldn't retrieve blob data");
    }

    jsize len = env->GetArrayLength(blobData);
    std::vector<uint8_t> data(len);
    env->GetByteArrayRegion(blobData, 0, len,
                            reinterpret_cast<jbyte *>(data.data()));
    env->DeleteLocalRef(blobData);
    return data;
  }

public:
  explicit AndroidPlatformContext(jobject blobModule)
      : _blobModule(blobModule) {}
  ~AndroidPlatformContext() {
    if (_blobModule) {
      JNIEnv *env = facebook::jni::Environment::current();
      env->DeleteGlobalRef(_blobModule);
      _blobModule = nullptr;
    }
  }

  wgpu::Surface makeSurface(wgpu::Instance instance, void *window, int width,
                            int height) override {
    wgpu::SurfaceSourceAndroidNativeWindow androidSurfaceDesc;
    androidSurfaceDesc.window = reinterpret_cast<ANativeWindow *>(window);
    wgpu::SurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &androidSurfaceDesc;
    return instance.CreateSurface(&surfaceDescriptor);
  }

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override {
    jni::Environment::ensureCurrentThreadIsAttached();

    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    auto data = resolveBlob(env, blobId, offset, size);
    return createImageBitmapFromData(data);
  }

  void
  createImageBitmapAsync(std::string blobId, double offset, double size,
                         std::function<void(ImageData)> onSuccess,
                         std::function<void(std::string)> onError) override {
    std::thread([this, blobId = std::move(blobId), offset, size,
                 onSuccess = std::move(onSuccess),
                 onError = std::move(onError)]() {
      jni::Environment::ensureCurrentThreadIsAttached();
      try {
        JNIEnv *env = facebook::jni::Environment::current();
        if (!env) {
          throw std::runtime_error("Couldn't get JNI environment");
        }
        auto data = resolveBlob(env, blobId, offset, size);
        auto result = createImageBitmapFromData(data);
        onSuccess(std::move(result));
      } catch (const std::exception &e) {
        onError(e.what());
      }
    }).detach();
  }

  ImageData createImageBitmapFromData(std::span<const uint8_t> data) override {
    jni::Environment::ensureCurrentThreadIsAttached();

    JNIEnv *env = facebook::jni::Environment::current();
    if (!env) {
      throw std::runtime_error("Couldn't get JNI environment");
    }

    // Create jbyteArray from the raw bytes
    jbyteArray byteArray = env->NewByteArray(static_cast<jsize>(data.size()));
    if (!byteArray) {
      throw std::runtime_error("Couldn't allocate byte array");
    }
    env->SetByteArrayRegion(byteArray, 0, static_cast<jsize>(data.size()),
                            reinterpret_cast<const jbyte *>(data.data()));

    // Decode via BitmapFactory
    jclass bitmapFactoryClass =
        env->FindClass("android/graphics/BitmapFactory");
    if (!bitmapFactoryClass) {
      env->DeleteLocalRef(byteArray);
      throw std::runtime_error("Couldn't find BitmapFactory class");
    }
    jmethodID decodeByteArrayMethod =
        env->GetStaticMethodID(bitmapFactoryClass, "decodeByteArray",
                               "([BII)Landroid/graphics/Bitmap;");
    if (!decodeByteArrayMethod) {
      env->DeleteLocalRef(byteArray);
      env->DeleteLocalRef(bitmapFactoryClass);
      throw std::runtime_error("Couldn't find decodeByteArray method");
    }
    jint length = static_cast<jint>(data.size());
    jobject bitmap = env->CallStaticObjectMethod(
        bitmapFactoryClass, decodeByteArrayMethod, byteArray, 0, length);
    env->DeleteLocalRef(bitmapFactoryClass);

    if (!bitmap) {
      env->DeleteLocalRef(byteArray);
      throw std::runtime_error("Couldn't decode image");
    }

    AndroidBitmapInfo bitmapInfo;
    if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) !=
        ANDROID_BITMAP_RESULT_SUCCESS) {
      env->DeleteLocalRef(byteArray);
      env->DeleteLocalRef(bitmap);
      throw std::runtime_error("Couldn't get bitmap info");
    }

    void *bitmapPixels;
    if (AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels) !=
        ANDROID_BITMAP_RESULT_SUCCESS) {
      env->DeleteLocalRef(byteArray);
      env->DeleteLocalRef(bitmap);
      throw std::runtime_error("Couldn't lock bitmap pixels");
    }

    ImageData result;
    result.width = static_cast<int>(bitmapInfo.width);
    result.height = static_cast<int>(bitmapInfo.height);
    result.data.resize(bitmapInfo.height * bitmapInfo.stride);
    memcpy(result.data.data(), bitmapPixels, result.data.size());

    AndroidBitmap_unlockPixels(env, bitmap);

    env->DeleteLocalRef(byteArray);
    env->DeleteLocalRef(bitmap);

    return result;
  }

  void createImageBitmapFromDataAsync(
      std::span<const uint8_t> data, std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) override {
    std::thread([this,
                 ownedData = std::vector<uint8_t>(data.begin(), data.end()),
                 onSuccess = std::move(onSuccess),
                 onError = std::move(onError)]() mutable {
      jni::Environment::ensureCurrentThreadIsAttached();
      try {
        auto result = createImageBitmapFromData(ownedData);
        onSuccess(std::move(result));
      } catch (const std::exception &e) {
        onError(e.what());
      }
    }).detach();
  }

  VideoFrameHandle loadVideoFrame(const std::string & /*path*/) override {
    // TODO: implement using MediaExtractor + MediaCodec to decode the first
    // frame into an AHardwareBuffer-backed Image (Android API 26+).
    throw std::runtime_error(
        "loadVideoFrame is not yet implemented on Android. Pass an "
        "AHardwareBuffer pointer obtained elsewhere (e.g. from "
        "react-native-vision-camera) directly to "
        "device.importSharedTextureMemory.");
  }

  VideoFrameHandle createTestVideoFrame(uint32_t width,
                                        uint32_t height) override {
    // Dawn's Android backend already requires API 26+, so AHardwareBuffer_*
    // symbols are guaranteed to be available at link time on supported builds.
    AHardwareBuffer_Desc desc = {};
    desc.width = width;
    desc.height = height;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY |
                 AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;

    AHardwareBuffer *buffer = nullptr;
    int err = AHardwareBuffer_allocate(&desc, &buffer);
    if (err != 0 || !buffer) {
      throw std::runtime_error(
          "createTestVideoFrame: AHardwareBuffer_allocate failed (" +
          std::to_string(err) + ")");
    }

    AHardwareBuffer_Desc actualDesc = {};
    AHardwareBuffer_describe(buffer, &actualDesc);
    const uint32_t stridePixels = actualDesc.stride;

    void *vaddr = nullptr;
    int rc = AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                                  -1, nullptr, &vaddr);
    if (rc != 0 || !vaddr) {
      AHardwareBuffer_release(buffer);
      throw std::runtime_error(
          "createTestVideoFrame: AHardwareBuffer_lock failed (" +
          std::to_string(rc) + ")");
    }

    // Same procedural pattern as ApplePlatformContext::createTestVideoFrame so
    // the shared snapshot matches on both platforms. Apple writes BGRA bytes
    // (CVPixelBuffer is kCVPixelFormatType_32BGRA, sampled as bgra8unorm);
    // here we write RGBA bytes (AHardwareBuffer is R8G8B8A8_UNORM, sampled as
    // rgba8unorm). The fragment shader sees the same logical (r, g, b, a).
    uint8_t *base = static_cast<uint8_t *>(vaddr);
    const size_t rowBytes = stridePixels * 4;
    for (uint32_t y = 0; y < height; ++y) {
      uint8_t *row = base + y * rowBytes;
      for (uint32_t x = 0; x < width; ++x) {
        uint8_t r = static_cast<uint8_t>((x * 255) / std::max(width - 1, 1u));
        uint8_t g = static_cast<uint8_t>((y * 255) / std::max(height - 1, 1u));
        uint8_t b = static_cast<uint8_t>(((x + y) & 0x20) ? 220 : 30);
        row[x * 4 + 0] = r;
        row[x * 4 + 1] = g;
        row[x * 4 + 2] = b;
        row[x * 4 + 3] = 0xFF;
      }
    }
    AHardwareBuffer_unlock(buffer, nullptr);

    VideoFrameHandle handle;
    handle.handle = static_cast<void *>(buffer);
    handle.width = width;
    handle.height = height;
    handle.deleter = [buffer]() { AHardwareBuffer_release(buffer); };
    return handle;
  }

  std::unique_ptr<IVideoPlayer>
  createVideoPlayer(const std::string & /*path*/) override {
    // TODO: implement using MediaCodec -> ImageReader (AHardwareBuffer mode).
    throw std::runtime_error(
        "createVideoPlayer is not yet implemented on Android.");
  }

  std::string writeTestVideoFile() override {
    // TODO: implement using MediaCodec (H.264 encoder) or MediaMuxer.
    throw std::runtime_error(
        "writeTestVideoFile is not yet implemented on Android.");
  }
};

} // namespace rnwgpu
