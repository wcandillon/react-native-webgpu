#pragma once

#include <android/bitmap.h>
#include <jni.h>

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

  void createImageBitmapAsync(
      std::string blobId, double offset, double size,
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
    std::thread([this, ownedData = std::vector<uint8_t>(data.begin(), data.end()),
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
};

} // namespace rnwgpu
